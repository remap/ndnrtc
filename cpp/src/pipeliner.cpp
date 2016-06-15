//
//  pipeliner.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "pipeliner.h"

#include "sample-estimator.h"
#include "frame-buffer.h"
#include "interest-control.h"
#include "interest-queue.h"
#include "segment-controller.h"

using namespace ndnrtc;
using namespace ndn;

Pipeliner::Pipeliner(const PipelinerSettings& settings):
sampleEstimator_(settings.sampleEstimator_),
buffer_(settings.buffer_),
interestControl_(settings.interestControl_),
interestQueue_(settings.interestQueue_),
playbackQueue_(settings.playbackQueue_),
segmentController_(settings.segmentController_),
interestLifetime_(settings.interestLifetimeMs_),
seqCounter_({0,0}),
nextSamplePriority_(SampleClass::Delta),
lastRequestedSample_(SampleClass::Delta)
{
    description_ = "pipeliner";
}

void
Pipeliner::express(const ndn::Name& threadPrefix)
{
    Name n(threadPrefix);
    n.append((nextSamplePriority_ == SampleClass::Delta ? NameComponents::NameComponentDelta : NameComponents::NameComponentKey));

    if (lastRequestedSample_ == SampleClass::Unknown) // request rightmost
    {
        boost::shared_ptr<Interest> interest(boost::make_shared<Interest>(n, interestLifetime_));
        interest->setMustBeFresh(true);
        interest->setChildSelector(1);
    
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
        n.appendSequenceNumber((nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_));
        request(getBatch(n, nextSamplePriority_), DeadlinePriority::fromNow(0));

        LogTraceC << "request sample " 
            << (nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_) 
            << " " << n << std::endl;
    }

    lastRequestedSample_ = nextSamplePriority_;
    nextSamplePriority_ = SampleClass::Delta;
}

void
Pipeliner::express(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests)
{
    LogDebugC << "request batch of size " << interests.size() << std::endl;

    request(interests, DeadlinePriority::fromNow(0));
}

void
Pipeliner::segmentArrived(const ndn::Name& threadPrefix)
{
    while (interestControl_->room() > 0)
    {
        Name n(threadPrefix);
        n.append((nextSamplePriority_ == SampleClass::Delta ? NameComponents::NameComponentDelta : NameComponents::NameComponentKey));
        n.appendSequenceNumber((nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_));

        const std::vector<boost::shared_ptr<const Interest>> batch = getBatch(n, nextSamplePriority_);
        int64_t deadline = playbackQueue_->size()+playbackQueue_->pendingSize();

        LogTraceC << "request sample " 
            << (nextSamplePriority_ == SampleClass::Delta ? seqCounter_.delta_ : seqCounter_.key_)
            << " " << n << "(" << batch.size() <<")" << std::endl;

        request(batch, DeadlinePriority::fromNow(deadline));
        buffer_->requested(batch);
        interestControl_->increment();

        if (nextSamplePriority_ == SampleClass::Delta) seqCounter_.delta_++;
        else seqCounter_.key_++;

        lastRequestedSample_ = nextSamplePriority_;
        nextSamplePriority_ = SampleClass::Delta;
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
        segmentController_->getOnTimeoutCallback());
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
