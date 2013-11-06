//
//  video-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#undef NDN_TRACE

#include "video-receiver.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this)),//, kRealtimePriority)),
playout_(false),
playoutFrameNo_(0),
playoutSleepIntervalUSec_(33333),
frameConsumer_(nullptr)
{
    playoutMeterId_ = NdnRtcUtils::setupFrequencyMeter();
    assemblerMeterId_ = NdnRtcUtils::setupFrequencyMeter();
    incomeFramesMeterId_ = NdnRtcUtils::setupFrequencyMeter();
}

NdnVideoReceiver::~NdnVideoReceiver()
{
}

//******************************************************************************
#pragma mark - public
int NdnVideoReceiver::init(shared_ptr<Face> face)
{
    int res = NdnMediaReceiver::init(face);
    
    if (RESULT_GOOD(res))
    {
        playoutSleepIntervalUSec_ =  1000000/ParamsStruct::validate(params_.producerRate, 1, MaxFrameRate, res, DefaultParams.producerRate);
        
        if (RESULT_NOT_OK(res))
            notifyError(RESULT_WARN, "bad producer rate value %d. using\
                        default instead - %d", params_.producerRate,
                        DefaultParams.producerRate);
    }
    
    return res;
}

int NdnVideoReceiver::startFetching()
{
    int res = NdnMediaReceiver::startFetching();
    
    if (RESULT_GOOD(res))
    {
        unsigned int tid = PLAYOUT_THREAD_ID;
    
        if (!playoutThread_.Start(tid))
        {
            notifyError(RESULT_ERR, "can't start playout thread");
            res = RESULT_ERR;
        }
    }
    
    return res;
}

int NdnVideoReceiver::stopFetching()
{
    TRACE("stop fetching");
  
    if (RESULT_FAIL(NdnMediaReceiver::stopFetching()))
        return RESULT_ERR;
        
    
    playout_ = false;
    playoutThread_.SetNotAlive();
    playoutThread_.Stop();
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization - ndn-cpp callbacks

//******************************************************************************
#pragma mark - private
bool NdnVideoReceiver::processPlayout()
{
    TRACE("run playout %d", playout_);
    if (playout_)
    {
        NdnRtcUtils::frequencyMeterTick(playoutMeterId_);
        
        int64_t processingDelay = NdnRtcUtils::microsecondTimestamp();
        
        shared_ptr<EncodedImage> encodedFrame = playoutBuffer_.acquireNextFrame();//(mode_==ReceiverModeFetch);
        
        if (encodedFrame.get() && frameConsumer_)
        {
            TRACE("push fetched frame to consumer");
            // lock slot until frame processing (decoding) is done
            frameConsumer_->onEncodedFrameDelivered(*encodedFrame);
        }
        else
        {
            TRACE("couldn't get frame with number %d", playoutFrameNo_);//playoutBuffer_.framePointer());
            playbackSkipped_++;
        }
        
        playoutBuffer_.releaseAcquiredFrame();
        playoutFrameNo_ = (mode_ == ReceiverModeFetch) ? playoutBuffer_.framePointer() : 0;
        processingDelay = NdnRtcUtils::microsecondTimestamp()-processingDelay;
        
        // sleep if needed
        if (playoutSleepIntervalUSec_-processingDelay > 0)
            usleep(playoutSleepIntervalUSec_-processingDelay);
    }
    
    return playout_;
}

void NdnVideoReceiver::switchToMode(NdnVideoReceiver::ReceiverMode mode)
{
    NdnMediaReceiver::switchToMode(mode);
    
    // check if we actually switched to another state
    if (mode == mode_)
        switch (mode_) {
            case ReceiverModeInit:
            {
                // start playout
                playout_ = true;
            }
                break;
            default:
                // nothing
                break;
        }
}

bool NdnVideoReceiver::isLate(unsigned int frameNo)
{
    if (mode_ == ReceiverModeFetch &&
        frameNo < playoutBuffer_.framePointer())
        return true;
    
    return false;
}
