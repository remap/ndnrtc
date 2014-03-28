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
#if 0
//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnVideoReceiver::NdnVideoReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params),
frameConsumer_(nullptr)
{
//    fetchAhead_ = 20; // fetch segments ahead
    keyNamespaceUsed_ = true;
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
    
    // after acquiring attempt, we could result in different state (i.e. if
    // rebuffering happened). check this
    if (mode_ == ReceiverModeFetch)
    {
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
            adjustedPlayoutTime = avSync_->synchronizePacket(slot, packetTsLocal,
                                                            this);
        
        if (framePlayoutTime >= 0 && adjustedPlayoutTime >= 0)
        {
            framePlayoutTime += adjustedPlayoutTime;
            jitterTiming_.updatePlayoutTime(framePlayoutTime);
            
            // setup and run playout timer for calculated playout interval
            jitterTiming_.runPlayoutTimer();
        }
    }
}

void NdnVideoReceiver::switchToMode(NdnVideoReceiver::ReceiverMode mode)
{
    NdnMediaReceiver::switchToMode(mode);
    
    // empty
}

bool NdnVideoReceiver::isLate(const Name &prefix, const unsigned char *segmentData,
                              int dataSz)
{
    return false;
#if 0
    PacketNumber frameNo = NdnRtcNamespace::getPacketNumber(prefix);
    SegmentNumber segmentNo = NdnRtcNamespace::getSegmentNumber(prefix);
    bool isKey = NdnRtcNamespace::isKeyFramePrefix(prefix);
    
    if (frameNo < 0)
    {
        LOG_TRACE("KEY FRAME. OLD_SCHOOL BREAKPOINT!!!!!!");
    }

        if (isKey && segmentNo == 0)
        {
            // retrieve real sequence frame number if it is possible
            PacketData::PacketMetadata meta;
#if 0
            TRACE("<==== DUMP2 %d (isKey: %s): %s",
                  frameNo,
                  isKey?"YES":"NO",
                  dump<41>((void*)segmentData).c_str());
#endif

            
            if (RESULT_NOT_FAIL(NdnFrameData::unpackMetadata(dataSz,
                                                             segmentData, meta)))
            {
                TRACE("sequence #: %d (%d booking id)", meta.sequencePacketNumber_, frameNo);
                
                if (playoutBuffer_->getState() != PlayoutBuffer::StatePlayback)
                {
                    if (playoutBuffer_->getJitterSize() == 0)
                    {
                        LOG_TRACE("LATE: zero jitter: %d < %d (%s)" ,
                                  meta.sequencePacketNumber_, firstFrame_,
                                  meta.sequencePacketNumber_ < firstFrame_ ? "TRUE":"FALSE");
                        
                        return meta.sequencePacketNumber_ < firstFrame_;
                    } // jitter is empty
                    else
                    {
                        LOG_TRACE("LATE: non-empty jitter: %d < %d (%s)" ,
                                  meta.sequencePacketNumber_,
                                  playoutBuffer_->framePointer(),
                                  meta.sequencePacketNumber_ < playoutBuffer_->framePointer() ? "TRUE":"FALSE");
                        
                        return meta.sequencePacketNumber_ < playoutBuffer_->framePointer();
                    } // jitter not empty
                } // check for playback state
                else
                {
                    LOG_TRACE("LATE: playback jitter: %d < %d (%s)",
                              meta.sequencePacketNumber_, playoutBuffer_->getPlayheadPointer(),
                              meta.sequencePacketNumber_ < playoutBuffer_->getPlayheadPointer() ? "TRUE" : "FALSE");
                    return (meta.sequencePacketNumber_ < playoutBuffer_->getPlayheadPointer());
                } // playing back
            } // get metadata
            
            return false;
        } // is key namespace

    return (frameNo < playoutBuffer_->getPlayheadPointer());
#endif
}

bool NdnVideoReceiver::needMoreKeyFrames()
{
//    return false;
    return (nKeyFramesPending_ < MinKeyFramesPending);
}
#endif