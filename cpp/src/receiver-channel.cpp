//
//  receiver-channel.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "receiver-channel.h"
#include "sender-channel.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//********************************************************************************
#pragma mark - construction/destruction
NdnReceiverChannel::NdnReceiverChannel(const ParamsStruct &params,
                                       const ParamsStruct &audioParams) :
NdnMediaChannel(params, audioParams),
localRender_(new NdnRenderer(1,params)),
decoder_(new NdnVideoDecoder(params)),
receiver_(new NdnVideoReceiver(params)),
audioReceiveChannel_(new NdnAudioReceiveChannel(NdnRtcUtils::sharedVoiceEngine()))
{
    localRender_->setObserver(this);
    decoder_->setObserver(this);
    receiver_->setObserver(this);
    audioReceiveChannel_->setObserver(this);
    
    receiver_->setFrameConsumer(decoder_.get());
    decoder_->setFrameConsumer(localRender_.get());
}

int NdnReceiverChannel::init()
{
    int res = NdnMediaChannel::init();
    
    if (RESULT_FAIL(res))
        return res;
    
    if (RESULT_FAIL(localRender_->init()))
        notifyError(RESULT_ERR, "can't intialize renderer");
    
    if (RESULT_FAIL(decoder_->init()))
        notifyError(RESULT_ERR, "can't intialize video decoder");
    
    if (RESULT_FAIL(receiver_->init(ndnFace_)))
        return notifyError(RESULT_ERR, "can't intialize video receiver");
    
    if (RESULT_FAIL(audioReceiveChannel_->init(audioParams_, ndnAudioFace_)))
        return notifyError(RESULT_ERR, "can't initialize audio receive channel");
    
    isInitialized_ = true;
    return 0;
}
int NdnReceiverChannel::startTransmission()
{
    int res = NdnMediaChannel::startTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    unsigned int tid = 1;
    
    if (RESULT_FAIL(localRender_->startRendering()))
        return notifyError(RESULT_ERR, "can't start render");
    
    if (RESULT_FAIL(receiver_->startFetching()))
        return notifyError(RESULT_ERR, "can't start fetching frames");
    
    if (RESULT_FAIL(audioReceiveChannel_->start()))
        return notifyError(RESULT_ERR, "can't start audio receive channel");
    
    isTransmitting_ = true;
    return RESULT_OK;
}
int NdnReceiverChannel::stopTransmission()
{
    int res = NdnMediaChannel::stopTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    receiver_->stopFetching();
    audioReceiveChannel_->stop();
    
    isTransmitting_ = false;
    return RESULT_OK;
}
void NdnReceiverChannel::getStat(ReceiverChannelStatistics &stat) const
{
    memset(&stat,0,sizeof(stat));
    
    stat.nPipeline_ = receiver_->getNPipelined();
    stat.nPlayback_ = receiver_->getNPlayout();
//    stat.nFetched_ = receiver_->getLatest(FrameBuffer::Slot::StateReady);
    stat.nLate_ = receiver_->getNLateFrames();
    
    stat.nSkipped_ = receiver_->getPlaybackSkipped();
//    stat.nTotalTimeouts_ = receiver_
//    stat.nTimeouts_ = receiver_
    
    stat.nFree_ = receiver_->getBufferStat(FrameBuffer::Slot::StateFree);
    stat.nLocked_ = receiver_->getBufferStat(FrameBuffer::Slot::StateLocked);
    stat.nAssembling_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    stat.nNew_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    
    stat.playoutFreq_  = receiver_->getPlayoutFreq();
    stat.inDataFreq_ = receiver_->getIncomeDataFreq();
    stat.inFramesFreq_ = receiver_->getIncomeFramesFreq();
}
