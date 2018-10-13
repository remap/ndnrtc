// 
// segment-fetcher.hpp
//
//  Created by Peter Gusev on 12 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __segment_fetcher_h__
#define __segment_fetcher_h__

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>

#include "ndnrtc-object.hpp"
#include "data-validator.hpp"

namespace ndn { 
	class Face;
	class Interest;
	class KeyChain;
	class Blob;
	class Name;
}

namespace ndnrtc {
	/**
	 * The code is borrowed from SegmentFetcher class of NDN-CPP library and data 
	 * packet validation behavior was altered.
	 *
 	 * SegmentFetcher is a utility class to the fetch latest version of segmented data.
 	 *
 	 * SegmentFetcher assumes that the data is named /<prefix>/<version>/<segment>,
 	 * where:
 	 * - <prefix> is the specified name prefix,
 	 * - <version> is an unknown version that needs to be discovered, and
 	 * - <segment> is a segment number. (The number of segments is unknown and is
 	 *   controlled by the `FinalBlockId` field in at least the last Data packet.
 	 *
 	 * The following logic is implemented in SegmentFetcher:
 	 *
 	 * 1. Express the first Interest to discover the version:
 	 *
 	 *    >> Interest: /<prefix>?ChildSelector=1&MustBeFresh=true
 	 *
 	 * 2. Infer the latest version of the Data: <version> = Data.getName().get(-2)
 	 *
 	 * 3. If the segment number in the retrieved packet == 0, go to step 5.
 	 *
 	 * 4. Send an Interest for segment 0:
 	 *
 	 *    >> Interest: /<prefix>/<version>/<segment=0>
 	 *
 	 * 5. Keep sending Interests for the next segment while the retrieved Data does
 	 *    not have a FinalBlockId or the FinalBlockId != Data.getName().get(-1).
 	 *
 	 *    >> Interest: /<prefix>/<version>/<segment=(N+1))>
 	 *
 	 * 6. Call the OnComplete callback with a blob that concatenates the content
 	 *    from all the segmented objects and vector with ValidationErrorInfo objects 
 	 *    which contain data packets that couldn't be verified.
 	 *
 	 * If an error occurs during the fetching process, the OnError callback is called
 	 * with a proper error code.  The following errors are possible:
 	 *
 	 * - `INTEREST_TIMEOUT`: if any of the Interests times out
 	 * - `DATA_HAS_NO_SEGMENT`: if any of the retrieved Data packets don't have a segment
 	 *   as the last component of the name (not counting the implicit digest)
 	 *
 	 * In order to validate individual segments, a VerifySegment callback needs to
 	 * be specified. If the callback returns false, the fetching process continues. 
 	 * If data validation is not required, the provided DontVerifySegment object 
 	 * can be used.
 	 *
 	 * Example:
 	 *     void onComplete(const Blob& encodedMessage);
 	 *
 	 *     void onError(SegmentFetcher::ErrorCode errorCode, const string& message);
 	 *
 	 *     Interest interest(Name("/data/prefix"));
 	 *     interest.setInterestLifetimeMilliseconds(1000);
 	 *
 	 *     SegmentFetcher.fetch
 	 *       (face, interest, SegmentFetcher::DontVerifySegment, onComplete, onError);
 	 */
	class SegmentFetcher : 	public NdnRtcComponent {
	public:
	  enum ErrorCode {
	    INTEREST_TIMEOUT = 1,
	    DATA_HAS_NO_SEGMENT = 2
	  };
	
	  typedef boost::function<bool(const boost::shared_ptr<ndn::Data>& data)> 
	  	VerifySegment;
	
	  typedef boost::function<void(const ndn::Blob& content, 
	  	const std::vector<ValidationErrorInfo>& info,
        const std::vector<boost::shared_ptr<ndn::Data>>& dataObjects)> OnComplete;
	
	  typedef boost::function<void
	    (ErrorCode errorCode, const std::string& message)> OnError;
	
	  /**
	   * DontVerifySegment may be used in fetch to skip validation of Data packets.
	   */
	  static bool
	  DontVerifySegment(const boost::shared_ptr<ndn::Data>& data);
	
	  /**
	   * Initiate segment fetching. For more details, see the documentation for
	   * the class.
	   * @param face This calls face.expressInterest to fetch more segments.
	   * @param baseInterest An Interest for the initial segment of the requested
	   * data, where baseInterest.getName() has the name prefix.
	   * This interest may include a custom InterestLifetime and selectors that will
	   * propagate to all subsequent Interests. The only exception is that the
	   * initial Interest will be forced to include selectors "ChildSelector=1" and
	   * "MustBeFresh=true" which will be turned off in subsequent Interests.
	   * @param verifySegment When a Data packet is received this calls
	   * verifySegment(data). If it returns false then abort fetching and call
	   * onError with SEGMENT_VERIFICATION_FAILED. If data validation is not
	   * required, use DontVerifySegment.
	   * NOTE: The library will log any exceptions thrown by this callback, but for
	   * better error handling the callback should catch and properly handle any
	   * exceptions.
	   * @param onComplete When all segments are received, call
	   * onComplete(content) where content is the concatenation of the content of
	   * all the segments.
	   * NOTE: The library will log any exceptions thrown by this callback, but for
	   * better error handling the callback should catch and properly handle any
	   * exceptions.
	   * @param onError Call onError(errorCode, message) for timeout or an error
	   * processing segments.
	   * NOTE: The library will log any exceptions thrown by this callback, but for
	   * better error handling the callback should catch and properly handle any
	   * exceptions.
	   */
	  static void
	  fetch
	    (ndn::Face& face, const ndn::Interest &baseInterest, const VerifySegment& verifySegment,
	     const OnComplete& onComplete, const OnError& onError);
	
	  /**
	   * Initiate segment fetching. For more details, see the documentation for
	   * the class.
	   * @param face This calls face.expressInterest to fetch more segments.
	   * @param baseInterest An Interest for the initial segment of the requested
	   * data, where baseInterest.getName() has the name prefix.
	   * This interest may include a custom InterestLifetime and selectors that will
	   * propagate to all subsequent Interests. The only exception is that the
	   * initial Interest will be forced to include selectors "ChildSelector=1" and
	   * "MustBeFresh=true" which will be turned off in subsequent Interests.
	   * @param validatorKeyChain When a Data packet is received this calls
	   * validatorKeyChain->verifyData(data). If validation fails then abort
	   * fetching and call onError with SEGMENT_VERIFICATION_FAILED. This does not
	   * make a copy of the KeyChain; the object must remain valid while fetching.
	   * If validatorKeyChain is null, this does not validate the data packet.
	   * @param onComplete When all segments are received, call
	   * onComplete(content) where content is the concatenation of the content of
	   * all the segments.
	   * NOTE: The library will log any exceptions thrown by this callback, but for
	   * better error handling the callback should catch and properly handle any
	   * exceptions.
	   * @param onError Call onError(errorCode, message) for timeout or an error
	   * processing segments.
	   * NOTE: The library will log any exceptions thrown by this callback, but for
	   * better error handling the callback should catch and properly handle any
	   * exceptions.
	   */
	  static void
	  fetch
	    (ndn::Face& face, const ndn::Interest &baseInterest, ndn::KeyChain* validatorKeyChain,
	     const OnComplete& onComplete, const OnError& onError);

	private:
	  /**
	   * Create a new SegmentFetcher to use the Face. See the static fetch method
	   * for details. If validatorKeyChain is not null, use it and ignore
	   * verifySegment. After creating the SegmentFetcher, call fetchFirstSegment.
	   */
	  SegmentFetcher
	    (ndn::Face& face, ndn::KeyChain* validatorKeyChain, const VerifySegment& verifySegment,
	     const OnComplete& onComplete, const OnError& onError)
	  : face_(face), validatorKeyChain_(validatorKeyChain), verifySegment_(verifySegment),
	    onComplete_(onComplete), onError_(onError)
	  {
	  }

	  void
	  fetchFirstSegment(const ndn::Interest& baseInterest);

	  void
	  fetchNextSegment
	    (const ndn::Interest& originalInterest, const ndn::Name& dataName, uint64_t segment);

	  void
	  onSegmentReceived
	    (const boost::shared_ptr<const ndn::Interest>& originalInterest,
	     const boost::shared_ptr<ndn::Data>& data);

	  void
	  processSegment
	    (const boost::shared_ptr<ndn::Data>& data,
	     const boost::shared_ptr<const ndn::Interest>& originalInterest);

	  void
	  onVerifyFailed(const boost::shared_ptr<ndn::Data>& data,
	  	const std::string& reason,
	  	const boost::shared_ptr<ndn::Data>& originalData,
	  	const boost::shared_ptr<const ndn::Interest>& originalInterest);

	  void
	  onTimeout(const boost::shared_ptr<const ndn::Interest>& interest);

	  /**
	   * Check if the last component in the name is a segment number.
	   * @param name The name to check.
	   * @return True if the name ends with a segment number, otherwise false.
	   */
	  static bool
	  endsWithSegmentNumber(const ndn::Name& name);

	  std::vector<ndn::Blob> contentParts_;
      std::vector<boost::shared_ptr<ndn::Data>> contentData_;
	  ndn::Face& face_;
	  ndn::KeyChain* validatorKeyChain_;
	  VerifySegment verifySegment_;
	  OnComplete onComplete_;
	  OnError onError_;
	  std::vector<ValidationErrorInfo> validationInfo_;
	};
}

#endif