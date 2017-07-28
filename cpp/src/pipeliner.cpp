//
//  pipeliner.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "pipeliner.hpp"
#include <ndn-cpp/exclude.hpp>

#include "sample-estimator.hpp"
#include "frame-buffer.hpp"
#include "interest-control.hpp"
#include "interest-queue.hpp"
#include "segment-controller.hpp"
#include "statistics.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

Pipeliner::Pipeliner(const PipelinerSettings& settings,
                     const boost::shared_ptr<INameScheme>& nameScheme):
nameScheme_(nameScheme),
sampleEstimator_(settings.sampleEstimator_),
buffer_(settings.buffer_),
interestControl_(settings.interestControl_),
interestQueue_(settings.interestQueue_),
playbackQueue_(settings.playbackQueue_),
segmentController_(settings.segmentController_),
interestLifetime_(settings.interestLifetimeMs_),
sstorage_(settings.sstorage_),
seqCounter_({0,0}),
nextSamplePriority_(SampleClass::Delta),
lastRequestedSample_(SampleClass::Delta)
{
    assert(sstorage_.get());
    
    description_ = "pipeliner";
    buffer_->attach(this);
}

Pipeliner::~Pipeliner()
{
    buffer_->detach(this);
}

void
Pipeliner::express(const ndn::Name& threadPrefix, bool placeInBuffer)
{
    if (lastRequestedSample_ == SampleClass::Unknown) // request rightmost
    {
        boost::shared_ptr<Interest> interest = nameScheme_->rightmostInterest(Name(threadPrefix), interestLifetime_);
    
        // if (exclusionPacket_)
        // {
        //     rightmostInterest_.getExclude().appendAny();
        //     rightmostInterest_.getExclude().appendComponent(NdnRtcUtils::componentFromInt(exclusionPacket_));
        //     exclusionPacket_ = 0;
        // }

        request(interest, DeadlinePriority::fromNow(0));

        LogDebugC << "request rightmost " << interest->getName() << std::endl;
    }
    else
    {
        Name n = nameScheme_->samplePrefix(threadPrefix, nextSamplePriority_);
        n.appendSequenceNumber((nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_));
        
        const std::vector<boost::shared_ptr<const Interest>> batch = getBatch(n, nextSamplePriority_);
        
        LogDebugC << "request sample "
            << (nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_) 
            << " " << SAMPLE_SUFFIX(n) << " batch size " << batch.size() << std::endl;

        request(batch, DeadlinePriority::fromNow(0));
        if (placeInBuffer) buffer_->requested(batch);
    }

    lastRequestedSample_ = nextSamplePriority_;
    nextSamplePriority_ = SampleClass::Delta;
}

void
Pipeliner::express(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests,
    bool placeInBuffer)
{
    LogTraceC << "request batch of size " << interests.size() << std::endl;

    request(interests, DeadlinePriority::fromNow(0));
    if (placeInBuffer) buffer_->requested(interests);
}

void
Pipeliner::segmentArrived(const ndn::Name& threadPrefix)
{
    if (interestControl_->room() > 0)
        LogDebugC << interestControl_->room()
            << " sample(s) will be requested: "
            << interestControl_->snapshot() << std::endl;
    
    while (interestControl_->room() > 0)
    {
        Name n = nameScheme_->samplePrefix(threadPrefix, nextSamplePriority_);
        n.appendSequenceNumber((nextSamplePriority_ == SampleClass::Delta ?
                                seqCounter_.delta_ : seqCounter_.key_));

        const std::vector<boost::shared_ptr<const Interest>> batch = getBatch(n, nextSamplePriority_);
        int64_t deadline = playbackQueue_->size()+playbackQueue_->pendingSize();

        request(batch, DeadlinePriority::fromNow(deadline));
        buffer_->requested(batch);
        interestControl_->increment();

        LogDebugC << "requested "
            << (nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_)
            << " " << SAMPLE_SUFFIX(n) << " x" << batch.size() << std::endl;
        
        if (nextSamplePriority_ == SampleClass::Delta) seqCounter_.delta_++;
        else seqCounter_.key_++;

        lastRequestedSample_ = nextSamplePriority_;
        nextSamplePriority_ = SampleClass::Delta;
        
        (*sstorage_)[Indicator::RequestedNum]++;
        if (lastRequestedSample_ == SampleClass::Key)
            (*sstorage_)[Indicator::RequestedKeyNum]++;
    }
}

void 
Pipeliner::reset()
{
    LogInfoC << "reset" << std::endl;

    seqCounter_ = {0,0};
    nextSamplePriority_ = SampleClass::Delta;
    lastRequestedSample_ = SampleClass::Unknown;
}

void 
Pipeliner::setSequenceNumber(PacketNumber seqNo, SampleClass cls)
{
    if (cls == SampleClass::Delta) seqCounter_.delta_ = seqNo;
    if (cls == SampleClass::Key) seqCounter_.key_ = seqNo;
}

PacketNumber 
Pipeliner::getSequenceNumber(SampleClass cls)
{
    if (cls == SampleClass::Delta) return seqCounter_.delta_;
    if (cls == SampleClass::Key) return seqCounter_.key_;
    return 0;
}


#pragma mark - private
void
Pipeliner::request(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests,
    const boost::shared_ptr<DeadlinePriority>& priority)
{
    for (auto& i:interests) request(i, priority);
}

void 
Pipeliner::request(const boost::shared_ptr<const ndn::Interest>& interest,
            const boost::shared_ptr<DeadlinePriority>& priority)
{
    interestQueue_->enqueueInterest(interest,  priority,
        segmentController_->getOnDataCallback(),
        segmentController_->getOnTimeoutCallback(),
        segmentController_->getOnNetworkNackCallback());
}

std::vector<boost::shared_ptr<const Interest>>
Pipeliner::getBatch(Name n, SampleClass cls, bool noParity) const
{
    std::vector<boost::shared_ptr<const Interest>> interests;
    unsigned int nData = sampleEstimator_->getSegmentNumberEstimation(cls, SegmentClass::Data);

    for (int segNo = 0; segNo < nData; ++segNo)
    {
        Name iname(n);
        iname.appendSegment(segNo);
        interests.push_back(boost::make_shared<const Interest>(iname, interestLifetime_));
    }

    if (!noParity)
    {
        n.append(NameComponents::NameComponentParity);
        unsigned int nParity = sampleEstimator_->getSegmentNumberEstimation(cls, SegmentClass::Parity);
        for (int segNo = 0; segNo < nParity; ++segNo)
        {
            Name iname(n);
            iname.appendSegment(segNo);
            interests.push_back(boost::make_shared<const Interest>(iname, interestLifetime_));
        }
    }

    return interests;
}

// IBufferObserver
void Pipeliner::onNewRequest(const boost::shared_ptr<BufferSlot>&)
{
    // do nothing
}

void Pipeliner::onNewData(const BufferReceipt& receipt)
{
    // check for missing segments
    std::vector<boost::shared_ptr<const Interest>> interests;
    for (auto& n:receipt.slot_->getMissingSegments())
        interests.push_back(boost::make_shared<const Interest>(n, interestLifetime_));
    if (interests.size())
    {
        LogTraceC << interests.size() << " missing segments for "
            << receipt.slot_->getNameInfo().getSuffix(suffix_filter::Thread) << std::endl;
        express(interests, true);
    }
    
    // set priority for requesting next key frame when key segment is received
    if (receipt.slot_->getNameInfo().class_ == SampleClass::Key &&
        receipt.slot_->getState() == BufferSlot::State::Assembling &&
        receipt.slot_->getFetchedNum() == 1)
        setNeedSample(SampleClass::Key);

}

Name
Pipeliner::VideoNameScheme::samplePrefix(const Name& threadPrefix, SampleClass cls)
{
    Name prefix(threadPrefix);
    if (cls == SampleClass::Delta)
        prefix.append(NameComponents::NameComponentDelta);
    else
        prefix.append(NameComponents::NameComponentKey);
    return prefix;
}

Name
Pipeliner::VideoNameScheme::rightmostPrefix(const ndn::Name& threadPrefix)
{
    Name prefix(threadPrefix);
    return prefix.append(NameComponents::NameComponentKey);
}

boost::shared_ptr<ndn::Interest>
Pipeliner::VideoNameScheme::rightmostInterest(const ndn::Name threadPrefix,
                                                   unsigned int lifetime)
{
    boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(rightmostPrefix(threadPrefix),
                                                                      lifetime));
    interest->setMustBeFresh(true);
    interest->setChildSelector(1);
    return interest;
}

Name
Pipeliner::AudioNameScheme::samplePrefix(const Name& threadPrefix, SampleClass cls)
{
    return threadPrefix;
    
}

Name
Pipeliner::AudioNameScheme::rightmostPrefix(const ndn::Name& threadPrefix)
{
    return threadPrefix;
}

boost::shared_ptr<ndn::Interest>
Pipeliner::AudioNameScheme::rightmostInterest(const ndn::Name threadPrefix,
                                              unsigned int lifetime)
{
    boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(rightmostPrefix(threadPrefix),
                                                                      lifetime));
    Exclude ex;
    ex.appendComponent(Name::Component(NameComponents::NameComponentMeta));
    interest->setExclude(ex);
    interest->setMustBeFresh(true);
    interest->setChildSelector(1);
    
    return interest;
}
