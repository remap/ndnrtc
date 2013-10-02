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
const string VideoReceiverParams::ParamNameProducerRate = "producer-rate";
const string VideoReceiverParams::ParamNameReceiverId = "receiver-id";

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(NdnParams *params) : NdnRtcObject(params),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this)),
pipelineThread_(*ThreadWrapper::CreateThread(pipelineThreadRoutine, this)),
assemblingThread_(*ThreadWrapper::CreateThread(assemblingThreadRoutine, this)),
mode_(ReceiverModeCreated),
playout_(false),
playoutFrameNo_(0),
playoutSleepIntervalUSec_(33333),
face_(nullptr),
frameConsumer_(nullptr)
{
    
}

NdnVideoReceiver::~NdnVideoReceiver()
{
    stopFetching();
}

//********************************************************************************
#pragma mark - public
int NdnVideoReceiver::init(shared_ptr<Face> face)
{
    mode_ = ReceiverModeInit;
    return 0;
}

int NdnVideoReceiver::startFetching()
{
    if (mode_ != ReceiverModeInit)
        return notifyError(-1, "receiver is not initialized or already started");
    
    // setup and start playout thread
    unsigned int tid = getParams()->getReceiverId()<<1+1;
    playoutSleepIntervalUSec_ = 1000000/getParams()->getProducerRate();
    
    if (!playoutThread_.Start(tid))
        return notifyError(-1, "can't start playout thread");
    
    playout_ = true;
    
    // setup and start assembling thread
    
    // setup and start pipeline thread
    
    mode_ = ReceiverModeWaitingFirstSegment;
    
    return 0;
}

int NdnVideoReceiver::stopFetching()
{
    if (mode_ == ReceiverModeCreated || mode_ == ReceiverModeInit)
        return 0; // notifyError(-1, "fetching was not started");
    
    // stop playout thread
    playoutThread_.SetNotAlive();
    playout_ = false;
    playoutThread_.Stop();
    
    // stop assembling thread
    
    // stop pipeline thread
    
    mode_ = ReceiverModeInit;
    
    return 0;
}

//********************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks
void NdnVideoReceiver::onTimeout(const shared_ptr<const Interest>& interest)
{
    TRACE("interest timeout: %s", interest.toUri().c_str());
}

void NdnVideoReceiver::onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
{
    TRACE("got data for interest: %s", interest.toUri().c_str());
}

//********************************************************************************
#pragma mark - private
bool NdnVideoReceiver::processPlayout()
{
#warning while or if?
//    while (playout_)
    if (playout_)
    {
        int64_t processingDelay = NdnRtcUtils::microsecondTimestamp();
        
        shared_ptr<EncodedImage> encodedFrame = playoutBuffer_.acquireNextFrame();
        
        if (encodedFrame.get() && frameConsumer_)
        {
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
        if (playoutSleepIntervalUSec_-processingDelay)
            usleep(playoutSleepIntervalUSec_-processingDelay);
    }

    return playout_;
}

bool NdnVideoReceiver::processInterests()
{
    return true;
}

bool NdnVideoReceiver::processAssembling()
{
    return true;
}
