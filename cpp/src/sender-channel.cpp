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

using namespace ndnrtc;
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
    if (!(videoInitialized_ = RESULT_GOOD(NdnMediaChannel::setupNdnNetwork(params_,
                                                                           DefaultParams, this,
                                                                           ndnFace_, ndnTransport_))))
        notifyError(RESULT_WARN, "can't initialize NDN networking for video channel");
    
    if (!(audioInitialized_ = RESULT_GOOD(NdnMediaChannel::setupNdnNetwork(audioParams_,
                                                                           DefaultParamsAudio,
                                                                           this, ndnAudioFace_,
                                                                           ndnAudioTransport_))))
        notifyError(RESULT_WARN, "can't initialize NDN networking for audio channel");
    
    if (!(videoInitialized_ || audioInitialized_))
        return notifyError(RESULT_ERR, "audio and video can not be initialized (ndn errors). aborting.");
    
    return RESULT_OK;
}
int NdnMediaChannel::startTransmission()
{
    if (!isInitialized_)
        return notifyError(RESULT_ERR, "sender channel was not initalized");
    
    return RESULT_OK;
}
int NdnMediaChannel::stopTransmission()
{
    if (!isTransmitting_)
        return notifyError(RESULT_ERR, "sender channel was not transmitting data");
    
    return RESULT_OK;
}
//******************************************************************************
#pragma mark - private
int NdnMediaChannel::setupNdnNetwork(const ParamsStruct &params,
                                     const ParamsStruct &defaultParams,
                                     NdnMediaChannel *callbackListener,
                                     shared_ptr<Face> &face,
                                     shared_ptr<ndn::Transport> &transport)
{
    int res = RESULT_OK;
    
    try
    {
        std::string host = (params.host)?string(params.host):string(defaultParams.host);
        int port = ParamsStruct::validateLE(params.portNum, MaxPortNum,
                                            res, defaultParams.portNum);
        
        if (RESULT_GOOD(res))
        {
            shared_ptr<ndn::Transport::ConnectionInfo>
            connInfo(new TcpTransport::ConnectionInfo(host.c_str(), port));
            
            transport.reset(new TcpTransport());
            face.reset(new Face(transport, connInfo));
            
            std::string streamAccessPrefix;
            res = MediaSender::getStreamKeyPrefix(params, streamAccessPrefix);
            
            if (RESULT_GOOD(res))
                face->registerPrefix(Name(streamAccessPrefix.c_str()),
                                     bind(&NdnMediaChannel::onInterest,
                                          callbackListener, _1, _2, _3),
                                     bind(&NdnMediaChannel::onRegisterFailed,
                                          callbackListener, _1));
        }
        else
        {
            LOG_NDNERROR("malformed parameters for host/port: %s, %d", params.host,
                         params.portNum);
            return RESULT_ERR;
        }
    }
    catch (std::exception &e)
    {
        LOG_NDNERROR("got error from ndn library: %s", e.what());
        return RESULT_ERR;
    }
    
    return res;
}

//******************************************************************************
#pragma mark - intefaces realization
void NdnMediaChannel::onInterest(const shared_ptr<const Name>& prefix,
                                 const shared_ptr<const Interest>& interest,
                                 ndn::Transport& transport)
{
    INFO("got interest: %s", interest->getName().toUri().c_str());
}

void
NdnMediaChannel::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    NDNERROR("failed to register prefix %s", prefix->toUri().c_str());
}

//******************************************************************************
//******************************************************************************
#pragma mark - construction/destruction
NdnSenderChannel::NdnSenderChannel(const ParamsStruct &params,
                                   const ParamsStruct &audioParams):
NdnMediaChannel(params, audioParams),
cc_(new CameraCapturer(params)),
localRender_(new NdnRenderer(0,params)),
coder_(new NdnVideoCoder(params)),
sender_(new NdnVideoSender(params)),
audioSendChannel_(new NdnAudioSendChannel(params, NdnRtcUtils::sharedVoiceEngine())),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliverEvent_(*EventWrapper::Create()),
processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                            kHighPriority))
{
    this->setLogger(new NdnLogger(NdnLoggerDetailLevelAll, "publish-%s.log",
                                  params.producerId));
    isLoggerCreated_ = true;
    
    cc_->setLogger(logger_);
    localRender_->setLogger(logger_);
    coder_->setLogger(logger_);
    
    cc_->setObserver(this);
    localRender_->setObserver(this);
    coder_->setObserver(this);
    
    // set connection capturer -> this
    cc_->setFrameConsumer(this);
    
    // set direct connection coder->sender
    coder_->setFrameConsumer(sender_.get());
    
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
    INFO("\tCAPTURED: \t%ld", sender_->getPacketNo());
    
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
    int res = NdnMediaChannel::init();
    
    if (RESULT_FAIL(res))
        return res;
    
    { // initialize video
        videoInitialized_ = RESULT_NOT_FAIL(cc_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize camera capturer");
        
        videoInitialized_ &= RESULT_NOT_FAIL(localRender_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize renderer");
        
        videoInitialized_ &= RESULT_NOT_FAIL(coder_->init());
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize video encoder");
        
        videoInitialized_ &= RESULT_NOT_FAIL(sender_->init(ndnTransport_));
        if (!videoInitialized_)
            notifyError(RESULT_WARN, "can't intialize video sender");
    }
    
    { // initialize audio
        audioInitialized_ = RESULT_NOT_FAIL(audioSendChannel_->init(ndnAudioTransport_));
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
    
    INFO("publishing initialized with video: %s, audio: %s",
         (videoInitialized_)?"yes":"no",
         (audioInitialized_)?"yes":"no");
    
    return (videoInitialized_&&audioInitialized_)?RESULT_OK:RESULT_WARN;
}

int NdnSenderChannel::startTransmission()
{
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
        
        videoTransmitting_ &= RESULT_NOT_FAIL(cc_->startCapture());
        if (!videoTransmitting_)
            notifyError(RESULT_WARN, "can't start camera capturer");
    }
    
    if (audioInitialized_)
    {
        audioTransmitting_ = RESULT_NOT_FAIL(audioSendChannel_->start());
        if (!audioTransmitting_)
            notifyError(RESULT_WARN, "can't start audio send channel");
    }
    
    isTransmitting_ = audioTransmitting_ || videoTransmitting_;
    
    if (!isTransmitting_)
        return notifyError(RESULT_ERR, "both audio and video can not be started."
                           " aborting.");
    
    INFO("publishing started with video: %s, audio: %s",
         (videoInitialized_)?"yes":"no",
         (audioInitialized_)?"yes":"no");
    
    return (audioTransmitting_&&videoTransmitting_)?RESULT_OK:RESULT_WARN;
}

int NdnSenderChannel::stopTransmission()
{
    int res = NdnMediaChannel::stopTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    if (videoTransmitting_)
    {
        videoTransmitting_ = false;
        
        if (cc_->isCapturing())
            cc_->stopCapture();
        
        processThread_.SetNotAlive();
        deliverEvent_.Set();
        
        if (!processThread_.Stop())
            notifyError(RESULT_WARN, "can't stop processing thread");
        
        localRender_->stopRendering();
    }
    
    if (audioTransmitting_)
    {
        audioTransmitting_ = false;
        audioSendChannel_->stop();
    }
    
    INFO("publishing stopped");
    
    isTransmitting_ = false;
    return RESULT_OK;
}

void NdnSenderChannel::getChannelStatistics(SenderChannelStatistics &stat)
{
    stat.videoStat_.nBytesPerSec_ = sender_->getDataRate();
    stat.videoStat_.nFramesPerSec_ = NdnRtcUtils::currentFrequencyMeterValue(frameFreqMeter_);
    stat.videoStat_.lastFrameNo_ = sender_->getFrameNo();
    stat.videoStat_.encodingRate_ = sender_->getCurrentPacketRate();
    stat.videoStat_.nDroppedByEncoder_ = coder_->getDroppedFramesNum();
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
            
            frameRate = currentRate + (rate-currentRate)*RateFilterAlpha;
        }
        TRACE("[SENDER] current frame rate: %f", frameRate);
        
        lastFrameStamp_ = now;
        
        deliver_cs_->Enter();
        if (!deliverFrame_.IsZeroSize()) {
            INFO("\tGRAB: \t%ld", sender_->getPacketNo());
            
            //(NdnRtcUtils::currentFrequencyMeterValue(frameFreqMeter_));
            
            uint64_t t = NdnRtcUtils::microsecondTimestamp();
            
            localRender_->onDeliverFrame(deliverFrame_);
            
            uint64_t t2 = NdnRtcUtils::microsecondTimestamp();
            
            INFO("\tRENDERED: \t%ld \t%ld",
                 sender_->getPacketNo(), t2 - t);
            coder_->onDeliverFrame(deliverFrame_);
            INFO("\tPUBLISHED: \t%ld \t%ld",
                 sender_->getFrameNo()-1,
                 NdnRtcUtils::microsecondTimestamp()-t2);
        }
        deliver_cs_->Leave();
    }
    // We're done!
    return true;
}
