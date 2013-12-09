//
//  video-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#undef NDN_TRACE

#include "video-receiver.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace webrtc;
using namespace ndnrtc;

unsigned int RebufferThreshold = 100000; // if N seconds were no frames - rebuffer

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this)),//, kRealtimePriority)),
playoutTimer_(*EventWrapper::Create()),
playout_(false),
playoutFrameNo_(0),
playoutSleepIntervalUSec_(33333),
frameConsumer_(nullptr)
{
    fetchAhead_ = 9; // fetch segments ahead
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
        if (RESULT_FAIL(playoutBuffer_.init(&frameBuffer_,
                                            DefaultParams.gop,
                                            DefaultParams.jitterSize)))
            return notifyError(RESULT_ERR, "could not initialize playout buffer");
        
        playoutBuffer_.registerBufferCallback(this);
        
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
        publisherRate_ = params_.producerRate;
        
        unsigned int tid = PLAYOUT_THREAD_ID;
        
        if (!playoutThread_.Start(tid))
        {
            notifyError(RESULT_ERR, "can't start playout thread");
            res = RESULT_ERR;
        }
    }
    
    if (RESULT_GOOD(res))
        INFO("video fetching started");
    
    return res;
}

int NdnVideoReceiver::stopFetching()
{
    TRACE("stop fetching");
  
    if (RESULT_FAIL(NdnMediaReceiver::stopFetching()))
        return RESULT_ERR;

    playoutTimer_.StopTimer();    
    playoutTimer_.Set();
    playout_ = false;
    playoutThread_.SetNotAlive();
    playoutThread_.Stop();
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization - IPlayoutBufferCallback
void NdnVideoReceiver::onFrameAddedToJitter(FrameBuffer::Slot *slot)
{
    unsigned int frameNo = slot->getFrameNumber();
    webrtc::VideoFrameType type = slot->getFrameType();
    NdnFrameData::FrameMetadata metadata;
    
    if (RESULT_GOOD(slot->getFrameMetadata(metadata)))
    {
        TRACE("updating parameters from received metadata: packetRate: %f",
              metadata.packetRate_);
        
        // we allow only slight deviations from originally provided rate
        if (metadata.packetRate_ != 0 &&
            metadata.packetRate_ >= params_.producerRate*(1-RateDeviation) &&
            metadata.packetRate_ <= params_.producerRate*(1+RateDeviation))
        {
            playoutSleepIntervalUSec_ = lround(1000000./metadata.packetRate_);
        }
        else
            playoutSleepIntervalUSec_ = lround(1000000./params_.producerRate);
        
        TRACE("[RECEIVER] new playout sleep interval: %ld", playoutSleepIntervalUSec_);
    }
    
    playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
    
    DBG("[VIDEO RECEVIER] received frame %d (type: %s). jitter size: %d",
        frameNo, (type == webrtc::kKeyFrame)?"KEY":"DELTA",
        playoutBuffer_.getJitterSize());
    
    nReceived_++;
    NdnRtcUtils::frequencyMeterTick(frameFrequencyMeter_);
    
    frameLogger_->log(NdnLoggerLevelInfo,"\tADDED: \t%d \t \t \t \t%d",
                      frameNo,
                      // empty
                      // empty
                      // empty
                      playoutBuffer_.getJitterSize());
}
void NdnVideoReceiver::onBufferStateChanged(PlayoutBuffer::State newState)
{
    playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
}
void NdnVideoReceiver::onMissedFrame(unsigned int frameNo)
{
    TRACE("VIDEO RECEIVER: playout missed frame %d. empty jitter occurence %d",
          frameNo, emptyJitterCounter_+1);
    
    if (!playoutBuffer_.getJitterSize())
        emptyJitterCounter_++;
    
    if (lastMissedFrameNo_ != frameNo)
    {
        lastMissedFrameNo_ = frameNo;
        nMissed_++;
    }
    
    frameLogger_->log(NdnLoggerLevelInfo,"\tMISSED: \t%d \t \t \t \t%d",
                      frameNo,
                      // empty
                      // empty
                      // empty
                      playoutBuffer_.getJitterSize());
}
void NdnVideoReceiver::onPlayheadMoved(unsigned int nextPlaybackFrame)
{
    TRACE("playhead moved. next playback frame is %d", nextPlaybackFrame);

    // should free all previous frames
    frameBuffer_.markSlotFree(nextPlaybackFrame-1);
}
//******************************************************************************
#pragma mark - private
bool NdnVideoReceiver::processPlayout()
{
    if (mode_ == ReceiverModeFetch)
    {
        if (NdnRtcUtils::millisecondTimestamp() - playoutLastUpdate_ >= RebufferThreshold)
            rebuffer();
        else
        {
            uint64_t now = NdnRtcUtils::microsecondTimestamp();
            
            FrameBuffer::Slot* slot = playoutBuffer_.acquireNextSlot();
            
            if (slot && frameConsumer_)
            {
                shared_ptr<EncodedImage> encodedFrame = slot->getFrame();
                NdnFrameData::FrameMetadata metadata;
                
                slot->getFrameMetadata(metadata);

                publisherRate_ = publisherRate_ + (metadata.packetRate_-publisherRate_)*RateFilterAlpha;

//                if (lastFrameTimeMs_ != 0)
//                {
//                    double rate = 1000/(encodedFrame->capture_time_ms_ - lastFrameTimeMs_);
//                    publisherRate_ = publisherRate_ + (rate-publisherRate_)*RateFilterAlpha;
//                }
                
//                lastFrameTimeMs_ = encodedFrame->capture_time_ms_;
                
                nPlayedOut_++;
                frameLogger_->log(NdnLoggerLevelInfo, "\tPLAYOUT: \t%d \t%d \t%d \t%d \t%d \t%ld",
                                  slot->getFrameNumber(),
                                  slot->assembledSegmentsNumber(),
                                  slot->totalSegmentsNumber(),
                                  slot->isKeyFrame(),
                                  playoutBuffer_.getJitterSize(),
                                  slot->getAssemblingTimeUsec());
                
                TRACE("push fetched frame to consumer. type %d",
                      encodedFrame->_frameType);
                frameConsumer_->onEncodedFrameDelivered(*encodedFrame);
            }
            
            playoutBuffer_.releaseAcquiredFrame();
            
            if (averageProcessingTimeUsec_ == 0)
                averageProcessingTimeUsec_ = NdnRtcUtils::microsecondTimestamp() - now;
            
            int waitTimeMs = (int)floor((1000000./publisherRate_-averageProcessingTimeUsec_)/1000.);
            
            TRACE("[PLAYOUT] processing %d, publisher rate: %f, timer for %d",
                  averageProcessingTimeUsec_, publisherRate_, waitTimeMs);
            
            if (waitTimeMs > 0)
            {
                uint64_t t = NdnRtcUtils::microsecondTimestamp();
                
                playoutTimer_.StartTimer(false, waitTimeMs);
                playout_ = (playoutTimer_.Wait(WEBRTC_EVENT_INFINITE) == webrtc::kEventSignaled);
                
                t = NdnRtcUtils::microsecondTimestamp() - t;
                TRACE("[PLAYOUT] waited on timer %ld usec. should - %d", t, waitTimeMs);
            }
            
            int fullProcessingTime = NdnRtcUtils::microsecondTimestamp()-now - waitTimeMs*1000;
            TRACE("full processing %ld", fullProcessingTime);
            
            averageProcessingTimeUsec_ = averageProcessingTimeUsec_ + (fullProcessingTime-averageProcessingTimeUsec_)*RateFilterAlpha;
            TRACE("new processing time %ld", averageProcessingTimeUsec_);
        }
    }
    
    return true; //playout_;
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
                break;
        }
}

bool NdnVideoReceiver::isLate(unsigned int frameNo)
{
    return (frameNo < playoutBuffer_.getPlayheadPointer());
}

unsigned int NdnVideoReceiver::getNextKeyFrameNo(unsigned int frameNo)
{
    unsigned int keyFrameNo = DefaultParams.gop*(int)ceil((double)frameNo/(double)DefaultParams.gop);
//    TRACE("next key frame number: %d (current is %d)", keyFrameNo, frameNo);
    
    return keyFrameNo;
}
