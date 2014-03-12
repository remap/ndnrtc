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
#include "ndnrtc-namespace.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
ndnrtc::new_api::Pipeliner::Pipeliner(const shared_ptr<const FetchChannel> &fetchChannel):
NdnRtcObject(fetchChannel->getParameters()),
mainThread_(*ThreadWrapper::CreateThread(Pipeliner::mainThreadRoutine, this)),
pipelineThread_(*ThreadWrapper::CreateThread(Pipeliner::pipelineThreadRoutin, this)),
pipelineTimer_(*EventWrapper::Create()),
fetchChannel_(fetchChannel),
isProcessing_(false)
{
    initialize();
}

ndnrtc::new_api::Pipeliner::~Pipeliner()
{
    
}

//******************************************************************************
#pragma mark - public
int
ndnrtc::new_api::Pipeliner::start()
{
    bufferEventsMask_ = ndnrtc::FrameBuffer::Event::StateChanged;
    isProcessing_ = true;
    
    unsigned int tid;
    mainThread_.Start(tid);
    
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::stop()
{
    mainThread_.SetNotAlive();
    isProcessing_ = false;
    mainThread_.Stop();
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - static

//******************************************************************************
#pragma mark - private
bool
ndnrtc::new_api::Pipeliner::processEvents()
{
    LogTrace(fetchChannel_->getLogFile()) << "wait buf events. mask " << bufferEventsMask_ << endl;
    
    ndnrtc::new_api::FrameBuffer::Event
    event = frameBuffer_->waitForEvents(bufferEventsMask_);
    
    LogTrace(fetchChannel_->getLogFile()) << "event "
        << FrameBuffer::Event::toString(event) << endl;
    
    switch (event.type_) {
        case FrameBuffer::Event::Error : {
            LogError(fetchChannel_->getLogFile()) << "error on buffer events";
            isProcessing_ = false;
        }
            break;
        default:
        {
            if (frameBuffer_->getState() == FrameBuffer::Valid)
                handleValidState(event);
            else
                handleInvalidState(event);
        }
            break;
    }
    
    return isProcessing_;
}

int
ndnrtc::new_api::Pipeliner::handleInvalidState
(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    LogTrace(fetchChannel_->getLogFile()) << "handle invalid buf state" << endl;
    
    vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>>
    activeSlots = frameBuffer_->getActiveSlots();
    
    if (activeSlots.size() == 0)
    {
        shared_ptr<Interest>
        rightmost = getInterestForRightMost(defaultInterestTimeoutMs_);
        
        express(*rightmost, defaultInterestTimeoutMs_);
        bufferEventsMask_ = FrameBuffer::Event::FirstSegment |
                            FrameBuffer::Event::Timeout;
    }
    else
        handleChase(event);
    
    
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::handleChase(const FrameBuffer::Event &event)
{
    if (event.type_ == FrameBuffer::Event::FirstSegment)
    {
        updateSegnumEstimation(event.slot_->getNamespace(),
                               event.slot_->getSegmentsNumber());
        
        int64_t lifetime = fetchChannel_->getParameters().interestTimeout*1000;
        requestMissing(event.slot_, lifetime, lifetime);
    }
    
    vector<shared_ptr<FrameBuffer::Slot>>
    activeSlots = fetchChannel_->getFrameBuffer()->getActiveSlots();
    
    if (activeSlots.size() == 1)
        initialDataArrived(event.slot_);
    
    if (activeSlots.size() > 1)
        chaseDataArrived(event);
    
    return RESULT_OK;
}

void
ndnrtc::new_api::Pipeliner::initialDataArrived
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>& slot)
{
    LogTrace(fetchChannel_->getLogFile()) << "first data arrived";
    
    // request next key frame
    requestKeyFrame(slot->getGopKeyFrameNumber()+1);
    bufferEstimator_->setProducerRate(slot->getPacketRate());
    double producerRate = (slot->getPacketRate())?slot->getPacketRate():params_.producerRate;
    
    startPipeliningDelta(slot->getSequentialNumber()+1, producerRate/2);
    
    bufferEventsMask_ = FrameBuffer::Event::FirstSegment | FrameBuffer::Event::Ready |
                            FrameBuffer::Event::Timeout;
}

void
ndnrtc::new_api::Pipeliner::chaseDataArrived(const FrameBuffer::Event& event)
{
    LogTrace(fetchChannel_->getLogFile()) << "" << endl;
    
    switch (event.type_) {
        case FrameBuffer::Event::FirstSegment:
        {
            chaseEstimation_->trackArrival();
        } // fall through
        case FrameBuffer::Event::Ready:
        {
//            chaseEstimation_->trackArrival();            
        }// fall through
        default:
        {
            pipelineIntervalMs_ = chaseEstimation_->getArrivalEstimation();
            
            if (event.slot_->getRecentSegment()->isOriginal())
            {
                LogTrace(fetchChannel_->getLogFile()) << "chased producer succesfully" << endl;
                
                frameBuffer_->freeSlotsLessThan(event.slot_->getPrefix());
            }
            
            if (chaseEstimation_->isArrivalStable())
            {
                LogTrace(fetchChannel_->getLogFile()) << "arrival delay stable" << endl;
                
                frameBuffer_->setState(FrameBuffer::Valid);
            }
            else
            {
#if 0
                if (frameBuffer_->getEstimatedBufferSize() >
                    frameBuffer_->getTargetSize())
                {
                    LogTrace(fetchChannel_->getLogFile())
                    << "buffer is too large - recycle old frames" << endl;
                    
                    fetchChannel_->getFrameBuffer()->recycleOldSlots();
                }
#endif
            }
        }
            break;
    }

}

int
ndnrtc::new_api::Pipeliner::handleValidState
(const ndnrtc::new_api::FrameBuffer::Event &event)
{
    LogTrace(fetchChannel_->getLogFile()) << "handle valid buf state" << endl;
    
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::scanBuffer()
{
    return RESULT_OK;
}

int
ndnrtc::new_api::Pipeliner::initialize()
{
    params_ = fetchChannel_->getParameters();
    defaultInterestTimeoutMs_ = params_.interestTimeout*1000;
    frameBuffer_ = fetchChannel_->getFrameBuffer();
    chaseEstimation_ = fetchChannel_->getChaseEstimation();
    bufferEstimator_ = fetchChannel_->getBufferEstimator();
    ndnAssembler_ = fetchChannel_->getPacketAssembler();
    
    shared_ptr<string>
    streamPrefixString = NdnRtcNamespace::getStreamPrefix(params_);
    
    shared_ptr<string>
    deltaPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    shared_ptr<string>
    keyPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_, true);
    
    if (!streamPrefixString.get() || !deltaPrefixString.get() ||
        !keyPrefixString.get())
        return notifyError(RESULT_ERR, "producer frames prefixes \
                           were not provided");

    streamPrefix_ = Name(streamPrefixString->c_str());
    deltaFramesPrefix_ = Name(deltaPrefixString->c_str());
    keyFramesPrefix_ = Name(keyPrefixString->c_str());
    
    return RESULT_OK;
}

shared_ptr<Interest>
ndnrtc::new_api::Pipeliner::getDefaultInterest(const ndn::Name &prefix)
{
    shared_ptr<Interest> interest(new Interest(prefix, fetchChannel_->getParameters().interestTimeout*1000));
    interest->setMustBeFresh(true);
    
    return interest;
}

shared_ptr<Interest>
ndnrtc::new_api::Pipeliner::getInterestForRightMost(int timeout,
                                                    bool isKeyNamespace,
                                                    PacketNumber exclude)
{
    Name prefix = isKeyNamespace?keyFramesPrefix_:deltaFramesPrefix_;
    shared_ptr<Interest> rightmost = getDefaultInterest(prefix);
    
    rightmost->setChildSelector(1);
    rightmost->setMinSuffixComponents(2);
    
    if (exclude >= 0)
    {
        rightmost->getExclude().appendAny();
        rightmost->getExclude().appendComponent(NdnRtcUtils::componentFromInt(exclude));
    }
    
    return rightmost;
}

void
ndnrtc::new_api::Pipeliner::updateSegnumEstimation(FrameBuffer::Slot::Namespace frameNs,
                                                   int nSegments)
{
    int estimatorId = (frameNs == FrameBuffer::Slot::Key)?
                        keySegnumEstimatorId_:deltaSegnumEstimatorId_;

    NdnRtcUtils::meanEstimatorNewValue(estimatorId, (double)nSegments);
}

void
ndnrtc::new_api::Pipeliner::requestKeyFrame(PacketNumber keyFrameNo)
{
    keyFrameSeqNo_ = keyFrameNo;
    
    Name framePrefix(keyFramesPrefix_);
    framePrefix.append(NdnRtcUtils::componentFromInt(keyFrameNo));

    Interest keyInterest(framePrefix);
    keyInterest.setInterestLifetimeMilliseconds(fetchChannel_->getParameters().interestTimeout*1000);
    expressRange(keyInterest, 0, NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_));
}

void
ndnrtc::new_api::Pipeliner::expressRange(Interest& interest,
                                         SegmentNumber startNo,
                                         SegmentNumber endNo, int64_t priority)
{
    Name prefix = interest.getName();
    
    for (SegmentNumber i = startNo; i <= endNo; i++)
    {
        Name segmentPrefix(prefix);
        segmentPrefix.appendSegment(i);
        interest.setName(segmentPrefix);
        
        express(interest, priority);
    }
}

void
ndnrtc::new_api::Pipeliner::express(const Interest &interest, int64_t priority)
{
    fetchChannel_->getInterestQueue()->enqueueInterest(interest,
                                                       Priority::fromArrivalDelay(priority),
                                                       ndnAssembler_->getOnDataHandler(),
                                                       ndnAssembler_->getOnTimeoutHandler());

    frameBuffer_->interestIssued(interest);
}

void
ndnrtc::new_api::Pipeliner::startPipeliningDelta(PacketNumber startPacketNo,
                                                 int64_t intervalMs)
{
    LogTrace(fetchChannel_->getLogFile()) << "start pipeline from "
        << startPacketNo << " interval = " << intervalMs << "ms" << endl;
    
    deltaFrameSeqNo_ = startPacketNo;
    pipelineIntervalMs_ = intervalMs;
    isPipelining_ = true;
    
    unsigned int tid;
    pipelineThread_.Start(tid);
}

void
ndnrtc::new_api::Pipeliner::stopPipeliningDelta()
{
    isPipelining_ = false;
    pipelineThread_.SetNotAlive();
    pipelineThread_.Stop();
}

bool
ndnrtc::new_api::Pipeliner::processPipeline()
{
    Name prefix(deltaFramesPrefix_);
    prefix.append(NdnRtcUtils::componentFromInt(deltaFrameSeqNo_));
    
    shared_ptr<Interest> deltaInterest = getDefaultInterest(prefix);
    
    expressRange(*deltaInterest, 0,
                 NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_));
    
    pipelineTimer_.StartTimer(false, pipelineIntervalMs_);
    pipelineTimer_.Wait(pipelineIntervalMs_);
    
    deltaFrameSeqNo_++;
    
    return isPipelining_;
}

void
ndnrtc::new_api::Pipeliner::requestMissing
(const shared_ptr<ndnrtc::new_api::FrameBuffer::Slot> &slot,
 int64_t lifetime, int64_t priority)
{
    Name prefix = (slot->getNamespace() == FrameBuffer::Slot::Key)?
                    Name(keyFramesPrefix_):Name(deltaFramesPrefix_);
    prefix.append(NdnRtcUtils::componentFromInt(slot->getSequentialNumber()));
    
    vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>
    missingSegments = slot->getMissingSegments();
    
    vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>::iterator it;
    for (it = missingSegments.begin(); it != missingSegments.end(); ++it)
    {
        shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> segment = *it;
        Name segmentPrefix(prefix);
        
        segmentPrefix.appendSegment(segment->getNumber());
        shared_ptr<Interest> segmentInterest = getDefaultInterest(segmentPrefix);
        segmentInterest->setInterestLifetimeMilliseconds(lifetime);
        express(*segmentInterest, priority);
    }
}
