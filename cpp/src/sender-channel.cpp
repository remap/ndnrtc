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

//******************************************************************************
#pragma mark - static
int NdnMediaChannel::getConnectHost(const ParamsStruct &params,
                                              std::string &host)
{
    int res = RESULT_OK;
    
    host = ParamsStruct::validate(params.host, DefaultParams.host, res);
    
    return res;
}

//******************************************************************************
#pragma mark - public
int NdnMediaChannel::init()
{
    int res = RESULT_OK;
    
    if (RESULT_FAIL(NdnMediaChannel::setupNdnNetwork(params_, DefaultParams, this,
                                                     ndnFace_, ndnTransport_)))
        return notifyError(RESULT_ERR, "can't initialize NDN networking for \
                           video publishing");
    
    if (RESULT_FAIL(NdnMediaChannel::setupNdnNetwork(audioParams_, DefaultParamsAudio, this,
                                                     ndnAudioFace_, ndnAudioTransport_)))
        return notifyError(RESULT_ERR, "can't initialize NDN networking for \
                           audio publishing");

    return res;
}
int NdnMediaChannel::startTransmission()
{
    if (!isInitialized_)
        return notifyError(RESULT_ERR, "sender channel was initalized");
    
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
        std::string host;
        int port = ParamsStruct::validateLE(params.portNum, MaxPortNum,
                                            res, defaultParams.portNum);
        res = NdnSenderChannel::getConnectHost(params, host);
        
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
            ERR("malformed parameters for host/port: %s, %d", params.host,
                params.portNum);
            return RESULT_ERR;
        }
    }
    catch (std::exception &e)
    {
        ERR("got error from ndn library: %s", e.what());
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
    ERR("failed to register prefix %s", prefix->toUri().c_str());
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
audioSendChannel_(new NdnAudioSendChannel(NdnRtcUtils::sharedVoiceEngine())),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliverEvent_(*EventWrapper::Create()),
processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                            kHighPriority))
{
    cc_->setObserver(this);
    localRender_->setObserver(this);
    coder_->setObserver(this);
    sender_->setObserver(this);
    audioSendChannel_->setObserver(this);
    
    // set connection capturer -> this
    cc_->setFrameConsumer(this);
    
    // set direct connection coder->sender
    coder_->setFrameConsumer(sender_.get());
    
    meterId_ = NdnRtcUtils::setupFrequencyMeter();
};
//******************************************************************************
#pragma mark - intefaces realization: IRawFrameConsumer
void NdnSenderChannel::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
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
    
    if (RESULT_FAIL((res = cc_->init())))
        notifyError(res, "can't intialize camera capturer");
    
    if (RESULT_FAIL((res = localRender_->init())))
        notifyError(res, "can't intialize renderer");
    
    if (RESULT_FAIL((res = coder_->init())))
        notifyError(res, "can't intialize video encoder");
    
    if (RESULT_FAIL((res = sender_->init(ndnTransport_))))
        return notifyError(res, "can't intialize video sender");
    
    if (RESULT_FAIL((res = audioSendChannel_->init(audioParams_,
                                                   ndnAudioTransport_))))
        return notifyError(res, "can't initialize audio send channel");
    
    isInitialized_ = true;
    return res;
}
int NdnSenderChannel::startTransmission()
{
    int res = NdnMediaChannel::startTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    unsigned int tid = 1;
    
    if (!processThread_.Start(tid))
        return notifyError(RESULT_ERR, "can't start processing thread");
    
    if (RESULT_FAIL(localRender_->startRendering()))
        return notifyError(RESULT_ERR, "can't start render");
    
    if (RESULT_FAIL(cc_->startCapture()))
        return notifyError(RESULT_ERR, "can't start camera capturer");
    
    if (RESULT_FAIL(audioSendChannel_->start()))
        return notifyError(RESULT_ERR, "can't start audio send channel");
    
    isTransmitting_ = true;
    return RESULT_OK;
}
int NdnSenderChannel::stopTransmission()
{
    int res = NdnMediaChannel::stopTransmission();
    
    if (RESULT_FAIL(res))
        return res;
    
    if (cc_->isCapturing())
        cc_->stopCapture();
    
    processThread_.SetNotAlive();
    deliverEvent_.Set();
    
    if (!processThread_.Stop())
        return notifyError(RESULT_ERR, "can't stop processing thread");
    
    audioSendChannel_->stop();
    
    isTransmitting_ = false;
    return RESULT_OK;
}
unsigned int NdnSenderChannel::getSentFramesNum()
{
    return sender_->getFrameNo();
}

//******************************************************************************
#pragma mark - private
bool NdnSenderChannel::process()
{
    if (deliverEvent_.Wait(100) == kEventSignaled) {
        NdnRtcUtils::frequencyMeterTick(meterId_);
        
        deliver_cs_->Enter();
        if (!deliverFrame_.IsZeroSize()) {
            TRACE("delivering frame");
            localRender_->onDeliverFrame(deliverFrame_);
            coder_->onDeliverFrame(deliverFrame_);
        }
        deliver_cs_->Leave();
    }
    // We're done!
    return true;
}
