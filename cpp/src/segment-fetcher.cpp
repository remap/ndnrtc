// 
// segment-fetcher.cpp
//
//  Created by Peter Gusev on 12 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdexcept>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/data.hpp>

#include "segment-fetcher.hpp"
#include "simple-log.hpp"

using namespace ndn;
using namespace std;
using namespace ndnrtc;
using namespace ndnlog::new_api;

bool
SegmentFetcher::DontVerifySegment(const boost::shared_ptr<Data>& data)
{
	return true;
}

void
SegmentFetcher::fetch
(Face& face, const Interest &baseInterest, const VerifySegment& verifySegment,
	const OnComplete& onComplete, const OnError& onError)
{
  // Make a shared_ptr because we make callbacks with bind using
  //   boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()) so the object remains allocated.
	boost::shared_ptr<SegmentFetcher> segmentFetcher
	(new SegmentFetcher(face, 0, verifySegment, onComplete, onError));
	segmentFetcher->fetchFirstSegment(baseInterest);
}

void
SegmentFetcher::fetch
(Face& face, const Interest &baseInterest, KeyChain* validatorKeyChain,
	const OnComplete& onComplete, const OnError& onError)
{
  // Make a shared_ptr because we make callbacks with bind using
  //   boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()) so the object remains allocated.
	boost::shared_ptr<SegmentFetcher> segmentFetcher
	(new SegmentFetcher
		(face, validatorKeyChain, SegmentFetcher::DontVerifySegment, onComplete,
			onError));
	segmentFetcher->fetchFirstSegment(baseInterest);
}

void
SegmentFetcher::fetchFirstSegment(const Interest& baseInterest)
{
	Interest interest(baseInterest);
	interest.setChildSelector(1);
	interest.setMustBeFresh(true);

	face_.expressInterest
	(interest,
		bind(&SegmentFetcher::onSegmentReceived, boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()), _1, _2),
		bind(&SegmentFetcher::onTimeout, boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()), _1));
}

void
SegmentFetcher::fetchNextSegment
(const Interest& originalInterest, const Name& dataName, uint64_t segment)
{
  // Start with the original Interest to preserve any special selectors.
	Interest interest(originalInterest);
  // Changing a field clears the nonce so that the library will generate a new one.
	interest.setChildSelector(0);
	interest.setMustBeFresh(false);
	interest.setName(dataName.getPrefix(-1).appendSegment(segment));

	face_.expressInterest
	(interest,
		bind(&SegmentFetcher::onSegmentReceived, boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()), _1, _2),
		bind(&SegmentFetcher::onTimeout, boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()), _1));
}

void
SegmentFetcher::onSegmentReceived
(const boost::shared_ptr<const Interest>& originalInterest,
	const boost::shared_ptr<Data>& data)
{
	if (validatorKeyChain_)
		validatorKeyChain_->verifyData
	(data,
		bind(&SegmentFetcher::processSegment, 
			boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()), _1, originalInterest),
		(const OnDataValidationFailed)bind(&SegmentFetcher::onVerifyFailed, 
			boost::dynamic_pointer_cast<SegmentFetcher>(shared_from_this()), _1, _2, data, originalInterest));
	else {
		if (!verifySegment_(data))
			onVerifyFailed(data, "User-defined verification failed", 
				data, originalInterest);

		processSegment(data, originalInterest);
	}
}

void
SegmentFetcher::processSegment
(const boost::shared_ptr<Data>& data,
	const boost::shared_ptr<const Interest>& originalInterest)
{
	if (!endsWithSegmentNumber(data->getName())) {
    // We don't expect a name without a segment number.  Treat it as a bad packet.
		try {
			onError_
			(DATA_HAS_NO_SEGMENT,
				string("Got an unexpected packet without a segment number: ") +
				data->getName().toUri());
		} catch (const std::exception& ex) {
			LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError: " << ex.what() << std::endl;
		} catch (...) {
			LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError." << std::endl;
		}
	}
	else {
		uint64_t currentSegment;
		try {
			currentSegment = data->getName().get(-1).toSegment();
		}
		catch (runtime_error& ex) {
			try {
				onError_
				(DATA_HAS_NO_SEGMENT,
					string("Error decoding the name segment number ") +
					data->getName().get(-1).toEscapedString() + ": " + ex.what());
			} catch (const std::exception& ex) {
				LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError: " << ex.what() << std::endl;
			} catch (...) {
				LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError." << std::endl;
			}
			return;
		}

		uint64_t expectedSegmentNumber = contentParts_.size();
		if (currentSegment != expectedSegmentNumber)
      // Try again to get the expected segment.  This also includes the case
      //   where the first segment is not segment 0.
			fetchNextSegment(*originalInterest, data->getName(), expectedSegmentNumber);
		else {
      // Save the content and check if we are finished.
			contentParts_.push_back(data->getContent());
            contentData_.push_back(data);

			if (data->getMetaInfo().getFinalBlockId().getValue().size() > 0) {
				uint64_t finalSegmentNumber;
				try {
					finalSegmentNumber = data->getMetaInfo().getFinalBlockId().toSegment();
				}
				catch (runtime_error& ex) {
					try {
						onError_
						(DATA_HAS_NO_SEGMENT,
							string("Error decoding the FinalBlockId segment number ") +
							data->getMetaInfo().getFinalBlockId().toEscapedString() + ": " +
							ex.what());
					} catch (const std::exception& ex) {
						LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError: " << ex.what() << std::endl;
					} catch (...) {
						LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError." << std::endl;
					}
					return;
				}

				if (currentSegment == finalSegmentNumber) {
          // We are finished.

          // Get the total size and concatenate to get the content.
					int totalSize = 0;
					for (size_t i = 0; i < contentParts_.size(); ++i)
						totalSize += contentParts_[i].size();
					boost::shared_ptr<vector<uint8_t> > content
					(new std::vector<uint8_t>(totalSize));
					size_t offset = 0;
					for (size_t i = 0; i < contentParts_.size(); ++i) {
						const Blob& part = contentParts_[i];
						memcpy(&(*content)[offset], part.buf(), part.size());
						offset += part.size();
					}

					try {
						onComplete_(Blob(content, false), validationInfo_, contentData_);
					} catch (const std::exception& ex) {
						LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onComplete: " << ex.what() << std::endl;
                        throw ;
					} catch (...) {
						LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onComplete." << std::endl;
                        throw ;
					}
					return;
				}
			}

      // Fetch the next segment.
			fetchNextSegment
			(*originalInterest, data->getName(), expectedSegmentNumber + 1);
		}
	}
}

void
SegmentFetcher::onVerifyFailed(const boost::shared_ptr<Data>& data,
	const std::string& reason,
	const boost::shared_ptr<Data>& originalData, 
	const boost::shared_ptr<const Interest>& originalInterest)
{
	try {
		LogWarnC << "Verification failed for " << data->getName() << ": " << reason << std::endl;

		validationInfo_.push_back(ValidationErrorInfo(data, reason));
		processSegment(originalData, originalInterest);
	} catch (const std::exception& ex) {
		LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError: " << ex.what() << std::endl;
	} catch (...) {
		LogErrorC << "SegmentFetcher::onSegmentReceived: Error in onError." << std::endl;
	}
}

void
SegmentFetcher::onTimeout(const boost::shared_ptr<const Interest>& interest)
{
	try {
		onError_
		(INTEREST_TIMEOUT,
			string("Time out for interest ") + interest->getName().toUri());
	} catch (const std::exception& ex) {
		LogErrorC << "SegmentFetcher::onTimeout: Error in onError: " << ex.what() << std::endl;
	} catch (...) {
		LogErrorC << "SegmentFetcher::onTimeout: Error in onError." << std::endl;
	}
}

bool
SegmentFetcher::endsWithSegmentNumber(const ndn::Name& name)
{
	    return name.size() >= 1 && name.get(-1).isSegment();
	  }
