//
//  sender-channel.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "sender-channel.h"
#include "video-receiver.h"

#define AUDIO_ID 0
#define VIDEO_ID 1

using namespace std;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace webrtc;

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnMediaChannel::NdnMediaChannel(const ParamsStruct &params,
                                 const ParamsStruct &audioParams):
NdnRtcObject(params),
audioParams_(audioParams)
{
}

NdnMediaChannel::~NdnMediaChannel()
{
}

//******************************************************************************
#pragma mark - static
//******************************************************************************
#pragma mark - public
int NdnMediaChannel::init()
{
    shared_ptr<string> userPrefix = NdnRtcNamespace::getUserPrefix(params_);
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(*userPrefix);
    
    videoFaceProcessor_ = FaceProcessor::createFaceProcessor(params_);
    
    if (!(videoInitialized_ = (videoFaceProcessor_.get() != nullptr)))
        notifyError(RESULT_WARN, "can't initialize NDN networking for video "
                    "channel");
    
    audioFaceProcessor_ = FaceProcessor::createFaceProcessor(params_);
    
    if (!(audioInitialized_ = (audioFaceProcessor_.get() != nullptr)))
        notifyError(RESULT_WARN, "can't initialize NDN networking for audio "
                    "channel");
    
    if (!(videoInitialized_ || audioInitialized_))
        return notifyError(RESULT_ERR, "audio and video can not be initialized "
                           "(ndn errors). aborting.");
    
    return RESULT_OK;
}
int NdnMediaChannel::startTransmission()
{
    if (!isInitialized_)
        return notifyError(RESULT_ERR, "sender channel was not initalized");
    
    if (videoInitialized_)
        videoFaceProcessor_->startProcessing();
    
    if (audioInitialized_)
        audioFaceProcessor_->startProcessing();
    
    return RESULT_OK;
}
int NdnMediaChannel::stopTransmission()
{
    if (!isTransmitting_)
        return notifyError(RESULT_ERR, "sender channel was not transmitting data");
    
    if (videoTransmitting_)
        videoFaceProcessor_->stopProcessing();
    
    if (audioTransmitting_)
        audioFaceProcessor_->stopProcessing();
    
    return RESULT_OK;
}
//******************************************************************************
#pragma mark - private

//******************************************************************************
#pragma mark - intefaces realization
void NdnMediaChannel::onInterest(const shared_ptr<const Name>& prefix,
                                 const shared_ptr<const Interest>& interest,
                                 ndn::Transport& transport)
{
    LogInfoC << "incoming interest "<< interest->getName() << endl;
}

void
NdnMediaChannel::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    notifyError(RESULT_ERR, "failed to register prefix %s", prefix->toUri().c_str());
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnSenderChannel::NdnSenderChannel(const ParamsStruct &params,
                                   const ParamsStruct &audioParams):
NdnMediaChannel(params, audioParams),
cameraCapturer_(new CameraCapturer(params)),
localRender_(new VideoRenderer(0,params)),
coder_(new NdnVideoCoder(params)),
sender_(new NdnVideoSender(params)),
audioCapturer_(new AudioCapturer(audioParams, NdnRtcUtils::sharedVoiceEngine())),
audioSender_(new NdnAudioSender(audioParams)),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliverEvent_(*EventWrapper::Create()),
processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                            kHighPriority))
{
    description_ = "send-channel";
    sender_->setObserver(this);
    audioSender_->setObserver(this);
    
    this->setLogger(new Logger(params.loggingLevel,
                               NdnRtcUtils::toString("producer-%s.log", params.producerId)));
    isLoggerCreated_ = true;
        
    cameraCapturer_->setObserver(this);
    localRender_->setObserver(this);
    coder_->setObserver(this);
    
    // set connection capturer -> this
    cameraCapturer_->setFrameConsumer(this);
    // set connection coder->sender
    coder_->setFrameConsumer(sender_.get());
    // set connection audioCapturer -> audioSender
    audioCapturer_->setFrameConsumer(audioSender_.get());
    
    frameFreqMeter_ = NdnRtcUtils::setupFrequencyMeter();
}

NdnSenderChannel::~NdnSenderChannel()
{
    NdnRtcUtils::releaseFrequencyMeter(frameFreqMeter_);
}

//******************************************************************************
#pragma mark - intefaces realization: IRawFrameConsumer
void NdnSenderChannel::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
    LogStatC << "captured\t" << sender_->getPacketNo() << endl;
    
    deliver_cs_->Enter();
    deliverFrame_.SwapFrame(&frame);
    deliver_cs_->Leave();
    
    deliverEvent_.Set();
};

//******************************************************************************
#pragma mark - all static

//******************************************************************************
#pragma mark - public
int NdnSenderChannel::init()
{
    LogInfoC << "starting initialization " << endl;
    LogInfoC << "unix timestamp: " << fixed << setprecision(6) << NdnRtcUtils::unixTimestamp() << endl;
    
    int res = NdnMediaChannel::init();
    
    if (RESULT_FAIL(res))
        return res;
    
    { // initialize video
        videoInitialized_ = RESULT_NOT_FAIL(cameraCapturer_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize camera capturer");
        
        videoInitialized_ &= RESULT_NOT_FAIL(localRender_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize renderer");
        
        videoInitialized_ &= RESULT_NOT_FAIL(coder_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize video encoder");
        
        videoInitialized_ &= RESULT_NOT_FAIL(sender_->init(videoFaceProcessor_, ndnKeyChain_));
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize video sender");
    }
    
    { // initialize audio
         audioInitialized_ = RESULT_NOT_FAIL(audioCapturer_->init());
        audioInitialized_ &= RESULT_NOT_FAIL(audioSender_->init(audioFaceProcessor_,
                                                                ndnKeyChain_));
        if (!audioInitialized_)
            notifyError(RESULT_WARN, "can't initialize audio send channel");
        
#ifdef AUDIO_OFF
        audioInitialized_ = false;
#endif
    }
    
    isInitialized_ = audioInitialized_||videoInitialized_;
    
    if (!isInitialized_)
        return notifyError(RESULT_ERR, "audio and video can not be initialized."
                           " aborting.");
    
    LogInfoC
    << "publishing initialized with video: "
    << ((videoInitialized_)?"yes":"no")
    << ", audio: " << ((audioInitialized_)?"yes":"no") << endl;
    
    return (videoInitialized_&&audioInitialized_)?RESULT_OK:RESULT_WARN;
}

int NdnSenderChannel::startTransmission()
{
    LogInfoC << "starting publishing" << endl;
    
    int res = NdnMediaChannel::startTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    unsigned int tid = 1;
    
    if (videoInitialized_)
    {
        videoTransmitting_ = (processThread_.Start(tid) != 0);
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start processing thread");
        
        videoTransmitting_ &= RESULT_NOT_FAIL(localRender_->startRendering("Local render"));
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start render");
        
        videoTransmitting_ &= RESULT_NOT_FAIL(cameraCapturer_->startCapture());
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start camera capturer");
    }
    
    if (audioInitialized_)
    {
        audioTransmitting_ = RESULT_NOT_FAIL(audioCapturer_->startCapture());
        
        if (!audioTransmitting_)
            notifyError(RESULT_WARN, "can't start audio send channel");
    }
    
    isTransmitting_ = audioTransmitting_ || videoTransmitting_;
    
    if (!isTransmitting_)
        return notifyError(RESULT_ERR, "both audio and video can not be started."
                           " aborting.");
    
    LogInfoC
    << "publishing started with video: "
    << ((videoInitialized_)?"yes":"no")
    << ", audio: " << ((audioInitialized_)?"yes":"no") << endl;
    
    return (audioTransmitting_&&videoTransmitting_)?RESULT_OK:RESULT_WARN;
}

int NdnSenderChannel::stopTransmission()
{
    LogInfoC << "stopping publishing" << endl;
    
    int res = NdnMediaChannel::stopTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    if (videoTransmitting_)
    {
        videoTransmitting_ = false;
        
        if (cameraCapturer_->isCapturing())
            cameraCapturer_->stopCapture();
        
        processThread_.SetNotAlive();
        deliverEvent_.Set();
        
        if (!processThread_.Stop())
            notifyError(RESULT_WARN, "can't stop processing thread");
        
        localRender_->stopRendering();
    }
    
    if (audioTransmitting_)
    {
        audioTransmitting_ = false;
        audioCapturer_->stopCapture();
        audioSender_->stop();
    }
    
    LogInfoC << "stopped" << endl;
    
    isTransmitting_ = false;
    return RESULT_OK;
}

void NdnSenderChannel::getChannelStatistics(SenderChannelStatistics &stat)
{
//    stat.videoStat_.nBytesPerSec_ = sender_->getDataRate();
    stat.videoStat_.nFramesPerSec_ = NdnRtcUtils::currentFrequencyMeterValue(frameFreqMeter_);
    stat.videoStat_.lastFrameNo_ = sender_->getFrameNo();
    stat.videoStat_.encodingRate_ = sender_->getCurrentPacketRate();
    stat.videoStat_.nDroppedByEncoder_ = coder_->getDroppedFramesNum();
    
    stat.audioStat_.lastFrameNo_ = audioSender_->getSampleNo();
    stat.audioStat_.encodingRate_ = audioSender_->getCurrentPacketRate();
}

void NdnSenderChannel::setLogger(ndnlog::new_api::Logger *logger)
{
    cameraCapturer_->setLogger(logger);
    localRender_->setLogger(logger);
    coder_->setLogger(logger);
    sender_->setLogger(logger);
    audioSender_->setLogger(logger);
    
    ILoggingObject::setLogger(logger);
}

//******************************************************************************
#pragma mark - private
bool NdnSenderChannel::process()
{
    if (deliverEvent_.Wait(100) == kEventSignaled) {
        NdnRtcUtils::frequencyMeterTick(frameFreqMeter_);
        
        double frameRate = params_.captureFramerate;
        uint64_t now = NdnRtcUtils::millisecondTimestamp();
        
        if (lastFrameStamp_ != 0)
        {
            double currentRate = sender_->getCurrentPacketRate();
            double rate = 1000./(now-lastFrameStamp_);
            
//            frameRate = currentRate + (rate-currentRate)*RateFilterAlpha;
        }
        LogStatC << "rate\t" << frameRate << endl;
        
        lastFrameStamp_ = now;
        
        deliver_cs_->Enter();
        if (!deliverFrame_.IsZeroSize()) {
            LogStatC << "grab\t" << sender_->getPacketNo() << endl;
            
            uint64_t t = NdnRtcUtils::microsecondTimestamp();
            
            localRender_->onDeliverFrame(deliverFrame_);
            
            uint64_t t2 = NdnRtcUtils::microsecondTimestamp();
            
            LogStatC
            << "rendered\t" << sender_->getPacketNo() << " "
            << t2 - t << endl;
            
            coder_->onDeliverFrame(deliverFrame_);
            
            LogStatC
            << "published\t"
            << sender_->getFrameNo()-1 << " "
            << NdnRtcUtils::microsecondTimestamp()-t2 << endl;
        }
        deliver_cs_->Leave();
    }

    return true;
}

