//
//  audio-receiver.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#undef NDN_LOGGING

#include "audio-receiver.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
NdnAudioReceiver::NdnAudioReceiver(const ParamsStruct &params) :
NdnMediaReceiver(params),
collectingThread_(*ThreadWrapper::CreateThread(collectingThreadRoutine, this))
{
    playoutBuffer_ = new PlayoutBuffer();
}

NdnAudioReceiver::~NdnAudioReceiver()
{
    stopFetching();
    delete playoutBuffer_;
}
//******************************************************************************
#pragma mark - public
int NdnAudioReceiver::init(shared_ptr<Face> face)
{
    int res = NdnMediaReceiver::init(face);
    
    if (RESULT_FAIL(playoutBuffer_->init(&frameBuffer_,
                                        DefaultParamsAudio.jitterSize)))
        return notifyError(RESULT_ERR, "could not initialize playout buffer");
    
    return res;
}

int NdnAudioReceiver::startFetching()
{
    if (RESULT_GOOD(NdnMediaReceiver::startFetching()))
    {
        unsigned int tid = AUDIO_THREAD_ID;
        
        isCollecting_ = true;
        
        if (!collectingThread_.Start(tid))
            return notifyError(RESULT_ERR, "can't start audio collecting thread");
    }
    else
        return RESULT_ERR;
    
    return RESULT_OK;
}

int NdnAudioReceiver::stopFetching()
{
    NdnMediaReceiver::stopFetching();
    
    isCollecting_ = false;
    
    collectingThread_.SetNotAlive();
    collectingThread_.Stop();
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization -

//******************************************************************************
#pragma mark - private
bool NdnAudioReceiver::collectAudioPackets()
{
    if (isCollecting_ && packetConsumer_)
    {
        FrameBuffer::Slot *slot = playoutBuffer_->acquireNextSlot();
        
        if (slot)
        {
            NdnAudioData::AudioPacket packet = slot->getAudioFrame();
            
            if (packet.length_ && packet.data_)
            {
                if (packet.isRTCP_)
                {
                    TRACE("pushing RTCP packet");
                    packetConsumer_->onRTCPPacketReceived(packet.length_,
                                                          packet.data_);
                }
                else
                {
                    TRACE("pushing RTP packet");
                    packetConsumer_->onRTPPacketReceived(packet.length_,
                                                         packet.data_);
                }
            }
            else
                WARN("got bad audio packet");
        }
        else
//            DBG("can't obtain next audio slot");
            ;
        TRACE("[AUDIO] playout buffer state: jitter size (%d), key frames (%d) "
              "last keyframe no (%d), top frame no (%d), diff (%d)",
              playoutBuffer_->getJitterSize(), playoutBuffer_->getNKeyFrames(),
              playoutBuffer_->getLastKeyFrameNo(), playoutBuffer_->getTopFrameNo(),
              playoutBuffer_->getTopFrameNo() - playoutBuffer_->getLastKeyFrameNo()
              );
        
        playoutBuffer_->releaseAcquiredFrame();
    }
    
    usleep(10000);
    return isCollecting_;
}

bool NdnAudioReceiver::isLate(unsigned int frameNo)
{
    if (mode_ == ReceiverModeFetch &&
        frameNo < playoutBuffer_->framePointer())
        return true;

    return false;
}

unsigned int NdnAudioReceiver::getNextKeyFrameNo(unsigned int frameNo)
{
    return frameNo;
}