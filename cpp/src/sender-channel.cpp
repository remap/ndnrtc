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
// NdnSenderChannel
#pragma mark - static
unsigned int NdnSenderChannel::getConnectHost(const ParamsStruct &params,
                                              std::string &host)
{
    int res = RESULT_OK;
    
    host = ParamsStruct::validate(params.host, DefaultParams.host, res);
    
    return res;
}

//******************************************************************************
#pragma mark - construction/destruction
NdnSenderChannel::NdnSenderChannel(const ParamsStruct &params):
NdnRtcObject(params),
cc_(new CameraCapturer(params)),
localRender_(new NdnRenderer(0,params)),
coder_(new NdnVideoCoder(params)),
sender_(new NdnVideoSender(params)),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliverEvent_(*EventWrapper::Create()),
processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                            kHighPriority))

{
    cc_->setObserver(this);
    localRender_->setObserver(this);
    coder_->setObserver(this);
    sender_->setObserver(this);
    
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
#pragma mark - intefaces realization
void NdnSenderChannel::onInterest(const shared_ptr<const Name>& prefix,
                                  const shared_ptr<const Interest>& interest,
                                  ndn::Transport& transport)
{
    INFO("got interest: %s", interest->getName().toUri().c_str());
}

void
NdnSenderChannel::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    ERR("failed to register prefix %s", prefix->toUri().c_str());
}

//******************************************************************************
#pragma mark - public
int NdnSenderChannel::init()
{
    int res = RESULT_OK;
    
    // connect to ndn
    try
    {
        std::string host;
        int port = ParamsStruct::validateLE(params_.portNum, MaxPortNum,
                                            res, DefaultParams.portNum);
        res = NdnSenderChannel::getConnectHost(params_, host);
        
        if (RESULT_GOOD(res))
        {
            shared_ptr<ndn::Transport::ConnectionInfo>
            connInfo(new TcpTransport::ConnectionInfo(host.c_str(), port));
            
            ndnTransport_.reset(new TcpTransport());
            ndnFace_.reset(new Face(ndnTransport_, connInfo));
            
            std::string streamAccessPrefix;
            res = MediaSender::getStreamKeyPrefix(params_, streamAccessPrefix);
            
            if (RESULT_GOOD(res))
                ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                         bind(&NdnSenderChannel::onInterest,
                                              this, _1, _2, _3),
                                         bind(&NdnSenderChannel::onRegisterFailed,
                                              this, _1));
            else
                notifyError(RESULT_ERR, "can't get prefix for registering: %s",
                            streamAccessPrefix.c_str());
        }
        else
            return notifyError(RESULT_ERR, "malformed parameters for\
                               host/port: %s, %d",
                               params_.host, params_.portNum);
    }
    catch (std::exception &e)
    {
        return notifyError(-1, "got error from ndn library: %s", e.what());
    }
    
    if (RESULT_FAIL((res = cc_->init())))
        notifyError(res, "can't intialize camera capturer");
    
    if (RESULT_FAIL((res = localRender_->init())))
        notifyError(res, "can't intialize renderer");
    
    if (RESULT_FAIL((res = coder_->init())))
        notifyError(res, "can't intialize video encoder");
    
    if (RESULT_FAIL((res = sender_->init(ndnTransport_))))
        return notifyError(res, "can't intialize video sender");
    
    return res;
}
int NdnSenderChannel::startTransmission()
{
    unsigned int tid = 1;
    
    if (!processThread_.Start(tid))
        return notifyError(RESULT_ERR, "can't start processing thread");
    
    if (localRender_->startRendering() < 0)
        return notifyError(RESULT_ERR, "can't start render");
    
    if (cc_->startCapture() < 0)
        return notifyError(RESULT_ERR, "can't start camera capturer");
    
    isTransmitting_ = true;
    return RESULT_OK;
}
int NdnSenderChannel::stopTransmission()
{
    if (cc_->isCapturing())
        cc_->stopCapture();
    
    processThread_.SetNotAlive();
    deliverEvent_.Set();

    isTransmitting_ = false;
    
    if (!processThread_.Stop())
        return notifyError(RESULT_ERR, "can't stop processing thread");

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