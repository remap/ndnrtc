//
//  video-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "video-receiver.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(NdnParams *params) : NdnRtcObject(params),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this)),
pipelineThread_(*ThreadWrapper::CreateThread(pipelineThreadRoutine, this)),
assemblingThread_(*ThreadWrapper::CreateThread(assemblingThreadRoutine, this)),
faceCs_(*CriticalSectionWrapper::CreateCriticalSection()),
mode_(ReceiverModeCreated),
playout_(false),
playoutFrameNo_(0),
playoutSleepIntervalUSec_(33333),
face_(nullptr),
frameConsumer_(nullptr)
{
    TRACE("");
}

NdnVideoReceiver::~NdnVideoReceiver()
{
    TRACE("");
    stopFetching();
}

//********************************************************************************
#pragma mark - public
int NdnVideoReceiver::init(shared_ptr<Face> face)
{
    switchToMode(ReceiverModeInit);
    
    string initialPrefix = getParams()->getStreamFramePrefix();
    
    if (initialPrefix == "")
        return notifyError(-1, "producer frames prefix was not provided");
    
    int bufSz = getParams()->getFrameBufferSize();
    int slotSz = getParams()->getFrameSlotSize();
    
    interestTimeout_ = getParams()->getDefaultTimeout();
    
    if (frameBuffer_.init(bufSz, slotSz) < 0)
        return notifyError(-1, "could not initialize frame buffer");
    else
        playoutBuffer_.init(&frameBuffer_);
    
    framesPrefix_ = Name(initialPrefix.c_str());
    producerSegmentSize_ = getParams()->getSegmentSize();
    face_ = face;
    
    return 0;
}

int NdnVideoReceiver::startFetching()
{
    if (mode_ != ReceiverModeInit)
        return notifyError(-1, "receiver is not initialized or has been already started");
    
    // setup and start playout thread
    unsigned int tid = PLAYOUT_THREAD_ID;
    int res = 0;
    
    switchToMode(ReceiverModeStarted);
    
    if (!playoutThread_.Start(tid))
    {
        notifyError(-1, "can't start playout thread");
        res = -1;
    }
    
    // setup and start assembling thread
    tid = ASSEMBLING_THREAD_ID;
    if (!assemblingThread_.Start(tid))
    {
        notifyError(-1, "can't start assembling thread");
        res = -1;
    }
    
    // setup and start pipeline thread
    tid = PIPELINER_THREAD_ID;
    if (!pipelineThread_.Start(tid))
    {
        notifyError(-1, "can't start playout thread");
        res = -1;
    }
    
    if (res < 0)
    {
        stopFetching();
        switchToMode(ReceiverModeInit);
    }
    
    return res;
}

int NdnVideoReceiver::stopFetching()
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
    
    playout_ = false;
    playoutThread_.SetNotAlive();
    assemblingThread_.SetNotAlive();
    pipelineThread_.SetNotAlive();
    TRACE("release buf");
    frameBuffer_.release();

    assemblingThread_.Stop();
    pipelineThread_.Stop();
    playoutThread_.Stop();
    TRACE("stopped threads");
    
    mode_ = ReceiverModeInit;
    
    return 0;
}

//********************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnVideoReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    Name prefix = interest->getName();
    
    if (isStreamInterest(prefix))
    {
        unsigned int frameNo = 0, segmentNo = 0;
        
        // check if it's a first segment
        if (prefix.getComponentCount() > framesPrefix_.getComponentCount())
        {
            unsigned int nComponents = prefix.getComponentCount();
            
            frameNo = NdnRtcUtils::frameNumber(prefix.getComponent(nComponents-2)),
            segmentNo = NdnRtcUtils::segmentNumber(prefix.getComponent(nComponents-1));
        }
        
//        TRACE("timeout: notify slot %d", frameNo);
        frameBuffer_.notifySegmentTimeout(frameNo, segmentNo);
    }
}

void NdnVideoReceiver::onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
{
    Name prefix = data->getName();
    TRACE("got data for the interest: %s", prefix.toUri().c_str());
    
    if (isStreamInterest(prefix))
    {
        unsigned int nComponents = prefix.getComponentCount();
        long frameNo = NdnRtcUtils::frameNumber(prefix.getComponent(framesPrefix_.getComponentCount())),
        segmentNo = NdnRtcUtils::segmentNumber(prefix.getComponent(framesPrefix_.getComponentCount()+1));
        
        if (frameNo >= 0 && segmentNo >= 0)
        {
            // check if its a very first segment
            if (mode_ == ReceiverModeWaitingFirstSegment)
            {
                TRACE("got first segment of size %d. rename slot: %d->%d",  data->getContent().size(), pipelinerFrameNo_, frameNo);
                frameBuffer_.renameSlot(pipelinerFrameNo_, frameNo);
            }
            
            // check if it's a first segment
            if (frameBuffer_.getState(frameNo) == FrameBuffer::Slot::StateNew)
            {
                TRACE("mark assembling");
                // get total number of segments
                Name::Component finalBlockID = data->getMetaInfo().getFinalBlockID();
                // final block id stores number of last segment, so the number of all segments is greater by 1
                unsigned int segmentsNum = NdnRtcUtils::segmentNumber(finalBlockID)+1;
                
                frameBuffer_.markSlotAssembling(frameNo, segmentsNum, producerSegmentSize_);
            }
            
            // finally, append data to the buffer
            frameBuffer_.appendSegment(frameNo, segmentNo,
                                       data->getContent().size(), (unsigned char*)data->getContent().buf());
        }
        else
        {
            WARN("got bad frame/segment numbers: %d (%d)", frameNo, segmentNo);
        }
    }
}

//********************************************************************************
#pragma mark - private
bool NdnVideoReceiver::processPlayout()
{
#warning while or if?
//    while (playout_)
    TRACE("run playout %d", playout_);
    if (playout_)
    {
        int64_t processingDelay = NdnRtcUtils::microsecondTimestamp();
        
        shared_ptr<EncodedImage> encodedFrame = playoutBuffer_.acquireNextFrame();
        
        if (encodedFrame.get() && frameConsumer_)
        {
            TRACE("push fetched frame to consumer");
            // lock slot until frame processing (decoding) is done
            frameConsumer_->onEncodedFrameDelivered(*encodedFrame);
        }
        else
        {
            WARN("couldn't get frame with number %d", playoutBuffer_.framePointer());
        }

        playoutBuffer_.releaseAcquiredFrame();
        processingDelay = NdnRtcUtils::microsecondTimestamp()-processingDelay;
        
        // sleep if needed
        if (playoutSleepIntervalUSec_-processingDelay > 0)
        {
            usleep(playoutSleepIntervalUSec_-processingDelay);
        }
    }
    
    return playout_;
}

bool NdnVideoReceiver::processInterests()
{
    bool res = true;
    FrameBuffer::Event ev = frameBuffer_.waitForEvents(pipelinerEventsMask_);
//    TRACE("pipeliner got event %d", ev.type_);
    bool lateFrame = ev.frameNo_ < playoutBuffer_.framePointer();
    
    switch (ev.type_)
    {
        case FrameBuffer::Event::EventTypeFreeSlot:
        {
            TRACE("book slot for frame %d", pipelinerFrameNo_);
            frameBuffer_.bookSlot(pipelinerFrameNo_);
            
            // need to check - whether we are just started and need to request first segment
            // if so, should not ask for booking more slots in buffer unless first segment is received
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
                pipelinerFrameNo_ = ev.frameNo_+1;
            }
            // pipeline interests for individual segments
            pipelineInterests(ev);
        }
            break;
        case FrameBuffer::Event::EventTypeTimeout:
        {
            // check wether we need to re-issue interest
            if (!lateFrame)
            {
//                WARN("got timeout for the frame %d (segment %d). re-issuing", ev.frameNo_, ev.segmentNo_);
                if (mode_ == ReceiverModeWaitingFirstSegment)
                    requestInitialSegment();
                else
                    requestSegment(ev.frameNo_,ev.segmentNo_);
            }
            else
            { // frame is out-dated
                TRACE("marking slot free %d", ev.frameNo_);
                frameBuffer_.markSlotFree(ev.frameNo_);
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
    }
    
//    if (lateFrame)
//    {
//        frameBuffer_.markSlotFree(ev.frameNo_);
//    }
    
    return res;
}

bool NdnVideoReceiver::processAssembling()
{
    // check if the first interest has been sent out
    // if not - skip processing events
    if (!(mode_ == ReceiverModeInit || mode_ == ReceiverModeCreated || mode_ == ReceiverModeStarted))
    {
        faceCs_.Enter();
        try {
            face_->processEvents();
        }
        catch(std::exception &e)
        {
            ERR("got exception from ndn-library while processing events: %s", e.what());
        }
        faceCs_.Leave();
    }

    usleep(10000);
        
    return true;
}

void NdnVideoReceiver::switchToMode(NdnVideoReceiver::ReceiverMode mode)
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
            
            // setup playout thread params
            playoutSleepIntervalUSec_ = 1000000/getParams()->getProducerRate();
            playout_ = true;
            INFO("switched to mode: Init");
        }
            break;
        case ReceiverModeWaitingFirstSegment:
        {
            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFirstSegment | FrameBuffer::Event::EventTypeTimeout;
            INFO("switched to mode: WaitingFirstSegment");
        }
            break;
        case ReceiverModeFetch:
        {
            pipelinerEventsMask_ = FrameBuffer::Event::EventTypeFreeSlot | FrameBuffer::Event::EventTypeFirstSegment | FrameBuffer::Event::EventTypeTimeout;
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

void NdnVideoReceiver::requestInitialSegment()
{
    Interest i(framesPrefix_, getParams()->getDefaultTimeout());

    i.setChildSelector(1); // seek for right most child
    i.setMinSuffixComponents(2);
    expressInterest(i);
}

void NdnVideoReceiver::pipelineInterests(FrameBuffer::Event &event)
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
    
    framePrefix.addComponent((const unsigned char*)frameNoStr.c_str(), frameNoStr.size());
    
    // 3. iteratively compute interst prefix and send out interest
    for (int i = 0; i < event.slot_->totalSegmentsNumber(); i++)
        if (i != event.segmentNo_) // send out only for segments we don't have yet
        {
            Name segmentPrefix = framePrefix;
            
            segmentPrefix.appendSegment(i);
            expressInterest(segmentPrefix);
        }
}

void NdnVideoReceiver::requestSegment(unsigned int frameNo, unsigned int segmentNo)
{
    Name segmentPrefix = framesPrefix_;
    stringstream ss;
    
    ss << frameNo;
    std::string frameNoStr = ss.str();
    
    segmentPrefix.addComponent((const unsigned char*)frameNoStr.c_str(), frameNoStr.size());
    segmentPrefix.appendSegment(segmentNo);
    
    expressInterest(segmentPrefix);
}

bool NdnVideoReceiver::isStreamInterest(Name prefix)
{
    return framesPrefix_.match(prefix);
}

void NdnVideoReceiver::expressInterest(Name &prefix)
{
    Interest i(prefix, interestTimeout_);
    
//    TRACE("expressing interest %s", i.getName().toUri().c_str());
    faceCs_.Enter();
    try{
    face_->expressInterest(i,
                           bind(&NdnVideoReceiver::onSegmentData, this, _1, _2),
                           bind(&NdnVideoReceiver::onTimeout, this, _1));
    }
    catch(std::exception &e)
    {
        ERR("got exception from ndn-library: %s (while expressing interest: %s)", e.what(),i.getName().toUri().c_str());
    }
    faceCs_.Leave();
}
void NdnVideoReceiver::expressInterest(Interest &i)
{
//    TRACE("expressing interest %s", i.getName().toUri().c_str());
    faceCs_.Enter();
    try {
        face_->expressInterest(i,
                               bind(&NdnVideoReceiver::onSegmentData, this, _1, _2),
                               bind(&NdnVideoReceiver::onTimeout, this, _1));
    }
    catch(std::exception &e)
    {
        ERR("got exception from ndn-library: %s (while expressing interest: %s)", e.what(),i.getName().toUri().c_str());
    }
    faceCs_.Leave();
}