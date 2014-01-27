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

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params),
frameConsumer_(nullptr)
{
    fetchAhead_ = 9; // fetch segments ahead
    playoutBuffer_ = new VideoPlayoutBuffer();
    frameLogger_ = new NdnLogger(NdnLoggerDetailLevelDefault,
                                 "fetch-vstat-%s.log", params.producerId);
    
    this->setLogger(new NdnLogger(NdnLoggerDetailLevelAll,
                                  "fetch-vchannel-%s.log", params.producerId));
    isLoggerCreated_ = true;
}

NdnVideoReceiver::~NdnVideoReceiver()
{
    delete frameLogger_;    
}

//******************************************************************************
#pragma mark - public
int NdnVideoReceiver::startFetching()
{
    int res = NdnMediaReceiver::startFetching();
    
    if (RESULT_GOOD(res))
        INFO("video fetching started");
    
    return res;
}

//******************************************************************************
#pragma mark - private
void NdnVideoReceiver::playbackPacket(int64_t packetTsLocal)
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
                          "PLAYOUT: \t%d \t%d \t%d \t%d \t%d \t%ld \t%ld \t%.2f \t%d",
                          slot->getFrameNumber(),
                          slot->assembledSegmentsNumber(),
                          slot->totalSegmentsNumber(),
                          slot->isKeyFrame(),
                          playoutBuffer_->getJitterSize(),
                          slot->getAssemblingTimeUsec(),
                          frame->capture_time_ms_,
                          currentProducerRate_,
                          pipelinerBufferSize_);
        
        frameConsumer_->onEncodedFrameDelivered(*frame.get());
    }
    
    // get playout time (delay) for the rendered frame
    int framePlayoutTime = ((VideoPlayoutBuffer*)playoutBuffer_)->releaseAcquiredSlot();
    int adjustedPlayoutTime = 0;
    
    // if av sync has been set up
    if (slot && avSync_.get())
        adjustedPlayoutTime= avSync_->synchronizePacket(slot, packetTsLocal);
    
    framePlayoutTime += adjustedPlayoutTime;
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
