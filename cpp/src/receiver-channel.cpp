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
audioReceiveChannel_(new NdnAudioReceiveChannel(audioParams, NdnRtcUtils::sharedVoiceEngine()))
{
    this->setLogger(new NdnLogger(NdnLoggerDetailLevelAll, "fetch-%s.log",
                                  params.producerId));
    isLoggerCreated_ = true;
    
    localRender_->setLogger(logger_);
    decoder_->setLogger(logger_);
    
    localRender_->setObserver(this);
    decoder_->setObserver(this);
    receiver_->setObserver(this);

    audioReceiveChannel_->setObserver(this);
    
    receiver_->setFrameConsumer(decoder_.get());
    decoder_->setFrameConsumer(localRender_.get());
    
#ifdef USE_AVSYNC
    shared_ptr<AudioVideoSynchronizer> avSync(new AudioVideoSynchronizer());
    avSync->setLogger(logger_);
    
    receiver_->setAVSynchronizer(avSync);
    audioReceiveChannel_->setAVSynchronizer(avSync);
#endif
}

int NdnReceiverChannel::init()
{
    int res = NdnMediaChannel::init();
    
    if (RESULT_FAIL(res))
        return res;
    
    { // initialize video
        videoInitialized_ = RESULT_NOT_FAIL(localRender_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't initialize local renderer for "
                        "incoming video");
        
        videoInitialized_ &= RESULT_NOT_FAIL(decoder_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't initialize decoder for incoming "
                        "video");
        
        videoInitialized_ &= RESULT_NOT_FAIL(receiver_->init(ndnFace_));
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't initialize ndn fetching for "
                        "incoming video");
#warning FOR TESTING ONLY! REMOVE THIS IN RELEASE VERSION!
#ifdef VIDEO_OFF
        videoInitialized_ = false;
#endif
    }
    
    { // initialize audio
        audioInitialized_ = RESULT_NOT_FAIL(audioReceiveChannel_->init(ndnAudioFace_));
        if (!audioInitialized_)
            notifyError(RESULT_WARN, "can't initialize audio receive channel");
        
#warning FOR TESTING ONLY! REMOVE THIS IN RELEASE VERSION!
#ifdef AUDIO_OFF
        audioInitialized_ = false;
#endif
    }
    
    isInitialized_ = audioInitialized_||videoInitialized_;
    
    if (!isInitialized_)
        return notifyError(RESULT_ERR, "both audio and video can not be initialized. aborting.");
    
    INFO("fetching initialized with video: %s, audio: %s",
         (videoInitialized_)?"yes":"no",
         (audioInitialized_)?"yes":"no");
    
    return (videoInitialized_&&audioInitialized_)?RESULT_OK:RESULT_WARN;
}
int NdnReceiverChannel::startTransmission()
{
    int res = NdnMediaChannel::startTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    unsigned int tid = 1;
    
    if (videoInitialized_)
    {
        videoTransmitting_ = RESULT_NOT_FAIL(localRender_->startRendering(string(params_.producerId)));
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start render");
        
        videoTransmitting_ &= RESULT_NOT_FAIL(receiver_->startFetching());
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start fetching frames from ndn");
    }
    
    if (audioInitialized_)
    {
        audioTransmitting_ = RESULT_NOT_FAIL(audioReceiveChannel_->start());
        if (!audioTransmitting_)
            notifyError(RESULT_WARN, "can't start audio receive channel");
    }
    
    isTransmitting_ = audioTransmitting_ || videoTransmitting_;
    
    if (!isTransmitting_)
        return notifyError(RESULT_ERR, "both audio and video fetching can not "
                           "be started. aborting");
    
    INFO("fetching started with video: %s, audio: %s",
         (videoInitialized_)?"yes":"no",
         (audioInitialized_)?"yes":"no");
    
    return (audioTransmitting_&&videoTransmitting_)?RESULT_OK:RESULT_WARN;
}
int NdnReceiverChannel::stopTransmission()
{
    int res = NdnMediaChannel::stopTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    if (videoTransmitting_)
    {
        videoTransmitting_ = false;
        
        localRender_->stopRendering();
        receiver_->stopFetching();
    }
    
    if (audioTransmitting_)
    {
        audioTransmitting_ = false;
        audioReceiveChannel_->stop();
    }
    
    INFO("fetching stopped");
    
    isTransmitting_ = false;
    return RESULT_OK;
}
void NdnReceiverChannel::getChannelStatistics(ReceiverChannelStatistics &stat)
{
    stat.videoStat_.nBytesPerSec_ = receiver_->getDataRate();
    stat.videoStat_.interestFrequency_ = receiver_->getInterestFrequency();
    stat.videoStat_.segmentsFrequency_ = receiver_->getSegmentFrequency();
    
    stat.videoStat_.rtt_ = receiver_->getLastRtt();
    stat.videoStat_.srtt_ = receiver_->getSrtt();
    
    stat.videoStat_.nPlayed_ = receiver_->getNPlayed();
    stat.videoStat_.nMissed_ = receiver_->getNMissed();
    stat.videoStat_.nLost_ = receiver_->getNLost();
    stat.videoStat_.nReceived_ = receiver_->getNReceived();
    
    stat.videoStat_.jitterSize_ = receiver_->getJitterOccupancy();
    stat.videoStat_.rebufferingEvents_ = receiver_->getRebufferingEvents();
    stat.videoStat_.actualProducerRate_ = receiver_->getActualProducerRate();
    
    stat.videoStat_.nSent_ = receiver_->getBufferStat(FrameBuffer::Slot::StateNew);
    stat.videoStat_.nAssembling_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    
    audioReceiveChannel_->getStatistics(stat.audioStat_);
}