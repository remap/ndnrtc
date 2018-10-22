//
// sample-validator.cpp
//
//  Created by Peter Gusev on 22 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "sample-validator.hpp"
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/name.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include "frame-data.hpp"
#include "name-components.hpp"
#include "meta-fetcher.hpp"

static const unsigned int META_FETCHER_POOL_SIZE = 100;

using namespace ndnrtc;
using namespace ndn;
using namespace ndnrtc::statistics;

void SampleValidator::onNewRequest(const boost::shared_ptr<BufferSlot> &slot)
{
    // do nothing
}

void SampleValidator::onNewData(const BufferReceipt &receipt)
{
    boost::shared_ptr<SampleValidator> me = boost::dynamic_pointer_cast<SampleValidator>(shared_from_this());
    boost::shared_ptr<int> nVerifiedSegments(boost::make_shared<int>(0));
    boost::shared_ptr<const BufferSlot> slot = receipt.slot_;
    keyChain_->verifyData(receipt.segment_->getData()->getData(),
                          [me, this, slot, nVerifiedSegments](const boost::shared_ptr<ndn::Data> &data) {
                              // success
                              (*nVerifiedSegments)++;
                              if (slot->getState() >= BufferSlot::State::Ready &&
                                  *nVerifiedSegments == slot->getFetchedNum() &&
                                  slot->verified_ == BufferSlot::Verification::Unknown)
                                  slot->verified_ = BufferSlot::Verification::Verified;

                              LogDebugC << "sample verified " << slot->dump() << std::endl;
                              (*statStorage_)[Indicator::VerifySuccess]++;
                          },
                          (const OnDataValidationFailed)([me, this, slot](const boost::shared_ptr<ndn::Data> &data, const std::string &reason) {
                              // failure
                              if (slot->getState() >= BufferSlot::State::Assembling)
                                  slot->verified_ = BufferSlot::Verification::Failed;

                              LogDebugC << "sample verification failure "
                                        << slot->getNameInfo().getSuffix(suffix_filter::Thread) << std::endl;
                              (*statStorage_)[Indicator::VerifyFailure]++;
                          }));
}

ManifestValidator::ManifestValidator(boost::shared_ptr<ndn::Face> face,
                                     boost::shared_ptr<ndn::KeyChain> keyChain,
                                     const boost::shared_ptr<StatisticsStorage> &statStorage) 
    : StatObject(statStorage), face_(face), keyChain_(keyChain),
    metaFetcherPool_(META_FETCHER_POOL_SIZE)
{
    description_ = "sample-validator";
}

void ManifestValidator::onNewRequest(const boost::shared_ptr<BufferSlot> &slot)
{
    if (slot->getState() == BufferSlot::State::New)
    {
        // initiate manifest fetching
        if (metaFetcherPool_.size() == 0)
            metaFetcherPool_.enlarge(META_FETCHER_POOL_SIZE);

        boost::shared_ptr<ManifestValidator> me = boost::dynamic_pointer_cast<ManifestValidator>(shared_from_this());
        boost::shared_ptr<MetaFetcher> mfetcher = metaFetcherPool_.pop();
        Name manifestName = slot->getNameInfo().getPrefix(prefix_filter::Sample).append(NameComponents::NameComponentManifest);
        mfetcher->fetch(face_, keyChain_,
                        manifestName,
                        [mfetcher, slot, me, this](NetworkData &nd, 
                                                   const std::vector<ValidationErrorInfo> info, 
                                                   const std::vector<boost::shared_ptr<Data>>&) {
                            if (info.size())
                            {
                                // had problems verifying manifest
                                for (auto &i : info)
                                    LogWarnC << "manifest verification failure " << i.getData()->getName()
                                             << " (KeyLocator " << (KeyLocator::getFromSignature(i.getData()->getSignature())).getKeyName()
                                             << "), reason: "
                                             << i.getReason() << std::endl;
                                slot->verified_ = BufferSlot::Verification::Failed;
                                (*me->statStorage_)[Indicator::VerifyFailure]++;
                            }
                            else
                            {
                                LogTraceC << "received manifest for "
                                          << slot->getNameInfo().getSuffix(suffix_filter::Thread) << std::endl;

                                if (slot->getState() >= BufferSlot::State::New)
                                {
                                    slot->manifest_ = boost::make_shared<Manifest>(boost::move(nd));
                                    if (slot->getState() >= BufferSlot::State::Ready)
                                        verifySlot(slot);
                                }
                                else
                                    LogWarnC << "late manifest arrival "
                                             << slot->getNameInfo().getSuffix(suffix_filter::Thread) << std::endl;
                            }
                            metaFetcherPool_.push(mfetcher);
                        },
                        [mfetcher, slot, me, this](const std::string &) {
                            LogErrorC << "couldn't fetch manifest for "
                                      << slot->getNameInfo().getSuffix(suffix_filter::Thread)
                                      << std::endl;

                            metaFetcherPool_.push(mfetcher);
                            (*me->statStorage_)[Indicator::VerifyFailure]++;
                        });

        LogTraceC << "fetch " << manifestName << std::endl;
    }
}

void ManifestValidator::onNewData(const BufferReceipt &receipt)
{
    if (receipt.slot_->getVerificationStatus() == BufferSlot::Verification::Unknown)
        if ((receipt.slot_->getState() & BufferSlot::State::Ready ||
             receipt.slot_->getState() & BufferSlot::State::Locked) &&
            receipt.slot_->manifest_.get())
            verifySlot(receipt.slot_);
}

void ManifestValidator::verifySlot(const boost::shared_ptr<const BufferSlot> slot)
{
    assert(slot->getState() >= BufferSlot::State::Ready);

    bool verified = true;
    for (auto &it : slot->fetched_)
        verified &= slot->manifest_->hasData(*(it.second->getData()->getData()));
    slot->verified_ = (verified ? BufferSlot::Verification::Verified : BufferSlot::Verification::Failed);

    if (slot->getVerificationStatus() == BufferSlot::Verification::Failed)
    {
        LogErrorC << "slot verification failure "
                  << slot->getNameInfo().getSuffix(suffix_filter::Thread) << std::endl;
        (*statStorage_)[Indicator::VerifyFailure]++;
    }
    else
    {
        LogDebugC << "verified " << slot->dump() << std::endl;
        (*statStorage_)[Indicator::VerifySuccess]++;
    }
}
