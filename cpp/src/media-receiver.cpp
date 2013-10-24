//
//  media-receiver.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "media-receiver.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
NdnMediaReceiver::NdnMediaReceiver(const ParamsStruct &params) :
NdnRtcObject(params),
pipelineThread_(*ThreadWrapper::CreateThread(pipelineThreadRoutine, this)),
assemblingThread_(*ThreadWrapper::CreateThread(assemblingThreadRoutine, this)),
faceCs_(*CriticalSectionWrapper::CreateCriticalSection()),
mode_(ReceiverModeCreated),
face_(nullptr)
{
    
}

NdnMediaReceiver::~NdnMediaReceiver()
{
    stopFetching();
}

//******************************************************************************
#pragma mark - public
int NdnMediaReceiver::init(shared_ptr<Face> face)
{
    switchToMode(ReceiverModeInit);
    
    string initialPrefix;
    
    if (RESULT_NOT_OK(MediaSender::getStreamFramePrefix(params_,
                                                        initialPrefix)))
        return notifyError(RESULT_ERR, "producer frames prefix \
                           was not provided");
    
    int bufSz = params_.bufferSize;
    int slotSz = params_.slotSize;
    
    interestTimeoutMs_ = params_.interestTimeout*1000;
    
    if (frameBuffer_.init(bufSz, slotSz) < 0)
        return notifyError(RESULT_ERR, "could not initialize frame buffer");
    
    framesPrefix_ = Name(initialPrefix.c_str());
    producerSegmentSize_ = params_.segmentSize;
    face_ = face;
    
    return 0;
}

int NdnMediaReceiver::startFetching()
{
    if (mode_ != ReceiverModeInit)
        return notifyError(RESULT_ERR, "receiver is not initialized or \
                           has been already started");
    
    int res = RESULT_OK;
    
    switchToMode(ReceiverModeStarted);
    
    // setup and start assembling thread
    unsigned int tid = ASSEMBLING_THREAD_ID;
    
    if (!assemblingThread_.Start(tid))
    {
        notifyError(RESULT_ERR, "can't start assembling thread");
        res = RESULT_ERR;
    }
    
    // setup and start pipeline thread
    tid = PIPELINER_THREAD_ID;
    if (!pipelineThread_.Start(tid))
    {
        notifyError(RESULT_ERR, "can't start playout thread");
        res = RESULT_ERR;
    }
    
    if (RESULT_FAIL(res))
    {
        stopFetching();
        switchToMode(ReceiverModeInit);
    }
    
    return res;
}

int NdnMediaReceiver::stopFetching()
{
    TRACE("stop fetching");
    
    if (mode_ == ReceiverModeCreated)
        return 0;
    
    if (mode_ == ReceiverModeInit)
    {
        TRACE("return on init");
        frameBuffer_.release();
        return 0;
    }
    
    assemblingThread_.SetNotAlive();
    pipelineThread_.SetNotAlive();
    TRACE("release buf");
    frameBuffer_.release();
    
    assemblingThread_.Stop();
    pipelineThread_.Stop();
    TRACE("stopped threads");
    
    mode_ = ReceiverModeInit;
    
    return 0;
}

//******************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnMediaReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    Name prefix = interest->getName();
    
    TRACE("got timeout for the interest: %s", prefix.toUri().c_str());
    
    if (isStreamInterest(prefix))
    {
        unsigned int frameNo = 0, segmentNo = 0;
        
        // check if it's a first segment
        if (prefix.getComponentCount() > framesPrefix_.getComponentCount())
        {
            unsigned int nComponents = prefix.getComponentCount(),
            frameNo =
            NdnRtcUtils::frameNumber(prefix.getComponent(nComponents-2)),
            
            segmentNo =
            NdnRtcUtils::segmentNumber(prefix.getComponent(nComponents-1));
        }
        
        if (isLate(frameNo))
        {
            TRACE("got timeout for late frame %d-%d", frameNo, segmentNo);
            frameBuffer_.markSlotFree(frameNo);
        }
        else
            frameBuffer_.notifySegmentTimeout(frameNo, segmentNo);
    }
    else
        WARN("got timeout for unexpected prefix");
}

void NdnMediaReceiver::onSegmentData(const shared_ptr<const Interest>& interest,
                                     const shared_ptr<Data>& data)
{
    Name prefix = data->getName();
    TRACE("got data for the interest: %s", prefix.toUri().c_str());
    
    if (isStreamInterest(prefix))
    {
//        NdnRtcUtils::frequencyMeterTick(assemblerMeterId_);
        
        unsigned int nComponents = prefix.getComponentCount();
        long frameNo =
        NdnRtcUtils::frameNumber(prefix.getComponent(framesPrefix_.getComponentCount())),
        
        segmentNo =
        NdnRtcUtils::segmentNumber(prefix.getComponent(framesPrefix_.getComponentCount()+1));
        
        if (frameNo >= 0 && segmentNo >= 0)
        {
            if (isLate(frameNo))
            {
                TRACE("got data for late frame %d-%d", frameNo, segmentNo);
                frameBuffer_.markSlotFree(frameNo);
                //                nLateFrames_++;
            }
            else
            {
                // check if it is a very first segment in a stream
                if (mode_ == ReceiverModeWaitingFirstSegment)
                {
                    TRACE("got first segment of size %d. rename slot: %d->%d",
                          data->getContent().size(),
                          pipelinerFrameNo_, frameNo);
                    
                    frameBuffer_.renameSlot(pipelinerFrameNo_, frameNo);
                }
                
                // check if it's a first segment
                if (frameBuffer_.getState(frameNo) ==
                    FrameBuffer::Slot::StateNew)
                {
                    TRACE("mark assembling");
                    // get total number of segments
                    Name::Component finalBlockID =
                    data->getMetaInfo().getFinalBlockID();

                    // final block id stores number of last segment, so the
                    // number of all segments is greater by 1
                    unsigned int segmentsNum =
                    NdnRtcUtils::segmentNumber(finalBlockID)+1;
                    
                    frameBuffer_.markSlotAssembling(frameNo, segmentsNum, producerSegmentSize_);
                }
                
                // finally, append data to the buffer
                FrameBuffer::CallResult res =
                frameBuffer_.appendSegment(frameNo, segmentNo,
                                           data->getContent().size(),
                                           (unsigned char*)data->getContent().buf());
                
//                if (res == FrameBuffer::CallResultReady)
//                    NdnRtcUtils::frequencyMeterTick(incomeFramesMeterId_);
            }
        }
        else
        {
            WARN("got bad frame/segment numbers: %d (%d)", frameNo, segmentNo);
        }
    }
    else
        WARN("got data with unexpected prefix");
}

//******************************************************************************
#pragma mark - private
bool NdnMediaReceiver::processInterests()
{
    bool res = true;
    FrameBuffer::Event ev = frameBuffer_.waitForEvents(pipelinerEventsMask_);
    bool lateFrame = isLate(ev.frameNo_);
    
    {
        switch (ev.type_)
        {
            case FrameBuffer::Event::EventTypeFreeSlot:
            {
                TRACE("book slot for frame %d", pipelinerFrameNo_);
                frameBuffer_.bookSlot(pipelinerFrameNo_);
                
                // need to check - whether we are just started and need to
                // request first segment
                // if so, should not ask for booking more slots in buffer unless
                // first segment is received
                if (mode_ == ReceiverModeStarted)
                {
                    switchToMode(ReceiverModeWaitingFirstSegment);
                    requestInitialSegment();
                }
                else
                {
                    // we've already in a fetching mode - request next frame
                    requestSegment(pipelinerFrameNo_, 0);
                    pipelinerFrameNo_++;
                }
            }
                break;
            case FrameBuffer::Event::EventTypeFirstSegment:
            {
                // check if it is a segment for the first frame
                if (mode_ == ReceiverModeWaitingFirstSegment)// && !lateFrame)
                {
                    // switch to fetching mode
                    switchToMode(ReceiverModeFetch);
#warning what to do with playoutFrameNo_ ???
//                    playoutFrameNo_ = ev.frameNo_;
                    pipelinerFrameNo_ = ev.frameNo_+1;
                }
                // pipeline interests for individual segments
                pipelineInterests(ev);
            }
                break;
            case FrameBuffer::Event::EventTypeTimeout:
            {
                // check wether we need to re-issue interest
                if (mode_ == ReceiverModeWaitingFirstSegment)
                {
                    TRACE("got timeout for initial interest");
                    requestInitialSegment();
                }
                else
                {
                    TRACE("framebuffer timeout - reissuing");
                    requestSegment(ev.frameNo_,ev.segmentNo_);
                }
            }
                break;
            case FrameBuffer::Event::EventTypeError:
            {
                ERR("got error event on frame buffer");
                res = false; // stop pipelining
            }
                break;
            default:
                ERR("got unexpected event: %d. ignoring", ev.type_);
                break;
        } // switch
    }
    
    return res;
}

bool NdnMediaReceiver::processAssembling()
{
    // check if the first interest has been sent out
    // if not - skip processing events
    if (!(mode_ == ReceiverModeInit ||
          mode_ == ReceiverModeCreated ||
          mode_ == ReceiverModeStarted))
    {
        faceCs_.Enter();
        try {
            face_->processEvents();
        }
        catch(std::exception &e)
        {
            ERR("got exception from ndn-library while processing events: %s",
                e.what());
        }
        faceCs_.Leave();
    }
    
    usleep(10000);
    
    return true;
}

void NdnMediaReceiver::switchToMode(NdnMediaReceiver::ReceiverMode mode)
{
    bool check = true;
    
    switch (mode) {
        case ReceiverModeCreated:
        {
            INFO("switched to mode: Created");
        }
            break;
        case ReceiverModeInit:
        {
            // setup pipeliner thread params
            pipelinerFrameNo_ = 0;
            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFreeSlot;
        }
            break;
        case ReceiverModeWaitingFirstSegment:
        {
            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFirstSegment |
            FrameBuffer::Event::EventTypeTimeout;
            INFO("switched to mode: WaitingFirstSegment");
        }
            break;
        case ReceiverModeFetch:
        {
            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFreeSlot |
            FrameBuffer::Event::EventTypeFirstSegment |
            FrameBuffer::Event::EventTypeTimeout;
            INFO("switched to mode: Fetch");
        }
            break;
        case ReceiverModeStarted:
        {
            INFO("switched to mode: Started");
        }
            break;
        case ReceiverModeChase:
            WARN("chase mode currently disabled");
            break;
        default:
            check = false;
            WARN("unable to switch to unknown mode %d", mode);
            break;
    }
    
    if (check)
        mode_ = mode;
}

void NdnMediaReceiver::requestInitialSegment()
{
    Interest i(framesPrefix_, interestTimeoutMs_);
    
    i.setChildSelector(1); // seek for right most child
    i.setMinSuffixComponents(2);
    expressInterest(i);
}

void NdnMediaReceiver::pipelineInterests(FrameBuffer::Event &event)
{
    TRACE("pipeline for the frame %d", event.frameNo_);
    
    // 1. get number of interests to be send out
    int interestsNum = event.slot_->totalSegmentsNumber() - 1;
    
    if (!interestsNum)
        return ;
    
    // 2. setup frame prefix
    Name framePrefix = framesPrefix_;
    stringstream ss;
    
    ss << event.frameNo_;
    std::string frameNoStr = ss.str();
    
    framePrefix.addComponent((const unsigned char*)frameNoStr.c_str(),
                             frameNoStr.size());
    
    // 3. iteratively compute interst prefix and send out interest
    for (int i = 0; i < event.slot_->totalSegmentsNumber(); i++)
        if (i != event.segmentNo_) // send out only for segments we don't have yet
        {
            Name segmentPrefix = framePrefix;
            
            segmentPrefix.appendSegment(i);
            expressInterest(segmentPrefix);
        }
}

void NdnMediaReceiver::requestSegment(unsigned int frameNo,
                                      unsigned int segmentNo)
{
    Name segmentPrefix = framesPrefix_;
    stringstream ss;
    
    ss << frameNo;
    std::string frameNoStr = ss.str();
    
    segmentPrefix.addComponent((const unsigned char*)frameNoStr.c_str(),
                               frameNoStr.size());
    segmentPrefix.appendSegment(segmentNo);
    
    expressInterest(segmentPrefix);
}

bool NdnMediaReceiver::isStreamInterest(Name prefix)
{
    return framesPrefix_.match(prefix);
}

void NdnMediaReceiver::expressInterest(Name &prefix)
{
    Interest i(prefix, interestTimeoutMs_);
    
    TRACE("expressing interest %s", i.getName().toUri().c_str());
    faceCs_.Enter();
    try{
        face_->expressInterest(i,
                               bind(&NdnMediaReceiver::onSegmentData, this, _1, _2),
                               bind(&NdnMediaReceiver::onTimeout, this, _1));
    }
    catch(std::exception &e)
    {
        ERR("got exception from ndn-library: %s \
            (while expressing interest: %s)",
            e.what(),i.getName().toUri().c_str());
    }
    faceCs_.Leave();
}

void NdnMediaReceiver::expressInterest(Interest &i)
{
    TRACE("expressing interest %s", i.getName().toUri().c_str());
    faceCs_.Enter();
    try {
        face_->expressInterest(i,
                               bind(&NdnMediaReceiver::onSegmentData, this, _1, _2),
                               bind(&NdnMediaReceiver::onTimeout, this, _1));
    }
    catch(std::exception &e)
    {
        ERR("got exception from ndn-library: %s \
            (while expressing interest: %s)",
            e.what(),i.getName().toUri().c_str());
    }
    faceCs_.Leave();
}

bool NdnMediaReceiver::isLate(unsigned int frameNo)
{
    return false;
}
