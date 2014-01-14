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

unsigned int RebufferThreshold = 3000; // if N seconds were no frames - rebuffer

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params),
playoutThread_(*ThreadWrapper::CreateThread(playoutThreadRoutine, this, kRealtimePriority)),
frameConsumer_(nullptr)
{
  fetchAhead_ = 9; // fetch segments ahead
  playoutBuffer_ = new VideoPlayoutBuffer();
}

NdnVideoReceiver::~NdnVideoReceiver()
{
  delete playoutBuffer_;
}

//******************************************************************************
#pragma mark - public
int NdnVideoReceiver::init(shared_ptr<Face> face)
{
  int res = NdnMediaReceiver::init(face);
  
  if (RESULT_GOOD(res))
  {
    publisherRate_ = ParamsStruct::validate(params_.producerRate, 1,
                                            MaxFrameRate, res,
                                            DefaultParams.producerRate);
    
    if (RESULT_FAIL(((VideoPlayoutBuffer*)playoutBuffer_)->init(&frameBuffer_,
                                                                DefaultParams.jitterSize,
                                                                publisherRate_)))
      return notifyError(RESULT_ERR, "could not initialize playout buffer");
    
    jitterTiming_.flush();
    playoutBuffer_->registerBufferCallback(this);
    
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
  
  if (RESULT_GOOD(res))
    INFO("video fetching started");
  
  return res;
}

int NdnVideoReceiver::stopFetching()
{
  TRACE("stop fetching");
  
  if (RESULT_FAIL(NdnMediaReceiver::stopFetching()))
    return RESULT_ERR;
  
  jitterTiming_.stop();
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
  
  // extract current producer rate
  NdnFrameData::FrameMetadata metadata;
  
  if (RESULT_GOOD(slot->getFrameMetadata(metadata)))
  {
    if (metadata.packetRate_ != 0 &&
        currentProducerRate_ != metadata.packetRate_)
    {
      currentProducerRate_ = metadata.packetRate_;
      
      int newJitterSize = NdnRtcUtils::toFrames(MinJitterSizeMs,
                                                currentProducerRate_);
      
      playoutBuffer_->setMinJitterSize(newJitterSize);
      DBG("[VIDEO RECEIVER] got updated producer rate %f. jitter size %d",
          currentProducerRate_, playoutBuffer_->getMinJitterSize());
    }
  }
  
  playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
  nReceived_++;
  emptyJitterCounter_ = 0;
  
  DBG("[VIDEO RECEVIER] received frame %d (type: %s). jitter size: %d",
      frameNo, (type == webrtc::kKeyFrame)?"KEY":"DELTA",
      playoutBuffer_->getJitterSize());
  frameLogger_->log(NdnLoggerLevelInfo,"ADDED: \t%d \t \t \t \t%d",
                    frameNo,
                    // empty
                    // empty
                    // empty
                    playoutBuffer_->getJitterSize());
}
void NdnVideoReceiver::onBufferStateChanged(PlayoutBuffer::State newState)
{
  playoutLastUpdate_ = NdnRtcUtils::millisecondTimestamp();
}
void NdnVideoReceiver::onMissedFrame(unsigned int frameNo)
{
  TRACE("[PLAYOUT] missed %d", frameNo);
  
  if (lastMissedFrameNo_ != frameNo)
  {
    lastMissedFrameNo_ = frameNo;
    nMissed_++;
  }
  
  frameLogger_->log(NdnLoggerLevelInfo,"MISSED: \t%d \t \t \t \t%d",
                    frameNo,
                    // empty
                    // empty
                    // empty
                    playoutBuffer_->getJitterSize());
}
void NdnVideoReceiver::onPlayheadMoved(unsigned int nextPlaybackFrame)
{
  // should free all previous frames
  frameBuffer_.markSlotFree(nextPlaybackFrame-1);
}
void NdnVideoReceiver::onJitterBufferUnderrun()
{
  TRACE("[PLAYOUT] UNDERRUN %d",
        emptyJitterCounter_+1);
  
  emptyJitterCounter_++;
  
  if (emptyJitterCounter_ >= MaxUnderrunNum)
  {
    emptyJitterCounter_ = 0;
    rebuffer();
  }
}
//******************************************************************************
#pragma mark - private
void NdnVideoReceiver::rebuffer()
{
  jitterTiming_.flush();
  NdnMediaReceiver::rebuffer();
}
bool NdnVideoReceiver::processPlayout()
{
  if (mode_ == ReceiverModeFetch)
  {
    uint64_t now = NdnRtcUtils::millisecondTimestamp();
    
    if (now - playoutLastUpdate_ >= RebufferThreshold)
      rebuffer();
    else if (playoutBuffer_->getState() == PlayoutBuffer::StatePlayback)
      playbackFrame();
  }
  
  return true; //playout_;
}

void NdnVideoReceiver::playbackFrame()
{
  jitterTiming_.startFramePlayout();
  
  int frameno = -1;
  FrameBuffer::Slot* slot;
  shared_ptr<EncodedImage> frame = ((VideoPlayoutBuffer*)playoutBuffer_)->acquireNextFrame(&slot);
  
  // render frame if we have one
  if (frame.get() && frameConsumer_)
  {
    frameno = slot->getFrameNumber();
    nPlayedOut_++;
    frameLogger_->log(NdnLoggerLevelInfo,
                      "PLAYOUT: \t%d \t%d \t%d \t%d \t%d \t%ld \t%ld \t%.2f",
                      slot->getFrameNumber(),
                      slot->assembledSegmentsNumber(),
                      slot->totalSegmentsNumber(),
                      slot->isKeyFrame(),
                      playoutBuffer_->getJitterSize(),
                      slot->getAssemblingTimeUsec(),
                      frame->capture_time_ms_,
                      currentProducerRate_);
    
    frameConsumer_->onEncodedFrameDelivered(*frame.get());
  }
  
  // get playout time (delay) for the rendered frame
  int framePlayoutTime = playoutBuffer_->releaseAcquiredFrame();
  jitterTiming_.updatePlayoutTime(framePlayoutTime);
  
  // setup and run playout timer for calculated playout interval
  jitterTiming_.runPlayoutTimer();
}

void NdnVideoReceiver::switchToMode(NdnVideoReceiver::ReceiverMode mode)
{
  NdnMediaReceiver::switchToMode(mode);
  
  // empty
}

bool NdnVideoReceiver::isLate(unsigned int frameNo)
{
  return (frameNo < playoutBuffer_->getPlayheadPointer());
}

unsigned int NdnVideoReceiver::getNextKeyFrameNo(unsigned int frameNo)
{
  unsigned int keyFrameNo = DefaultParams.gop*(int)ceil((double)frameNo/(double)DefaultParams.gop);
  
  return keyFrameNo;
}
