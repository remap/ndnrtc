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

//********************************************************************************
/**
 * @name NdnSenderChannel class
 */
//********************************************************************************
#pragma mark - construction/destruction
NdnSenderChannel::NdnSenderChannel(NdnParams* params):NdnRtcObject(params),
cc_(new CameraCapturer(params)),
localRender_(new NdnRenderer(0,params)),
coder_(new NdnVideoCoder(params)),
sender_(new NdnVideoSender(params)),
deliver_cs_(CriticalSectionWrapper::CreateCriticalSection()),
deliverEvent_(*EventWrapper::Create()),
processThread_(*ThreadWrapper::CreateThread(processDeliveredFrame, this,  kHighPriority))

{
    cc_->setObserver(this);
    localRender_->setObserver(this);
    coder_->setObserver(this);
    sender_->setObserver(this);
    
    // set connection capturer -> this
    cc_->setFrameConsumer(this);
    
    // set direct connection coder->sender
    coder_->setFrameConsumer(sender_.get());
};
//********************************************************************************
#pragma mark - intefaces realization: IRawFrameConsumer
void NdnSenderChannel::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
    deliver_cs_->Enter();
    deliverFrame_.SwapFrame(&frame);
    deliver_cs_->Leave();
    
    deliverEvent_.Set();
    
//    timestamp += 90000/30.;
//
//    webrtc::I420VideoFrame *frame_copy = new webrtc::I420VideoFrame();
//    frame_copy->CopyFrame(frame);
//    frame_copy->set_timestamp(timestamp);
    
    // pass frame for rendering and encoding

    
//    nsCOMPtr<nsRunnable> encoderTask = new MozEncodingTask(frame, coder_.get());
//    NS_DispatchToMainThread(encoderTask);
//    encodingThread_->Dispatch(encoderTask, nsIThread::DISPATCH_NORMAL);
    
};

//********************************************************************************
#pragma mark - all static

//********************************************************************************
#pragma mark - intefaces realization
void NdnSenderChannel::onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, ndn::Transport& transport)
{
    INFO("got interest: %s", interest->getName().toUri().c_str());
}

void NdnSenderChannel::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    ERR("failed to register prefix %s", prefix->toUri().c_str());
}

//********************************************************************************
#pragma mark - public
int NdnSenderChannel::init()
{
    // connect to ndn
    try
    {
        std::string host = getParams()->getConnectHost();
        int port = getParams()->getConnectPort();
        
        if (host != "" && port > 0)
        {
            shared_ptr<ndn::Transport::ConnectionInfo> connInfo(new TcpTransport::ConnectionInfo(host.c_str(), port));
            
            ndnTransport_.reset(new TcpTransport());
            ndnFace_.reset(new Face(ndnTransport_, connInfo));
            
            std::string streamAccessPrefix = ((VideoSenderParams*)getParams())->getStreamKeyPrefix();
            ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                     bind(&NdnSenderChannel::onInterest, this, _1, _2, _3),
                                     bind(&NdnSenderChannel::onRegisterFailed, this, _1));
        }
        else
            return notifyError(-1, "malformed parameters for host/port");
    }
    catch (std::exception &e)
    {
        return notifyError(-1, "got error from ndn library: %s", e.what());
    }
    
    if (cc_->init() < 0)
        notifyError(-1, "can't intialize camera capturer");
    
    cc_->printCapturingInfo();
    
    if (localRender_->init() < 0)
        notifyError(-1, "can't intialize renderer");
    
    if (coder_->init() < 0)
        notifyError(-1, "can't intialize video encoder");
    
    if (sender_->init(ndnTransport_) < 0)
        return notifyError(-1, "can't intialize video sender");
    
    return 0;
}
int NdnSenderChannel::startTransmission()
{
    TRACE("stopping transmission");
    
    unsigned int tid = 1;
    
    if (!processThread_.Start(tid))
        return notifyError(-1, "can't start processing thread");
    
    if (localRender_->startRendering() < 0)
        return notifyError(-1, "can't start render");
    
    if (cc_->startCapture() < 0)
        return notifyError(-1, "can't start camera capturer");
    
    isTransmitting_ = true;
    return 0;
}
int NdnSenderChannel::stopTransmission()
{
    if (cc_->isCapturing())
        cc_->stopCapture();
    
    processThread_.SetNotAlive();
    deliverEvent_.Set();

    isTransmitting_ = false;
    
    if (!processThread_.Stop())
        return notifyError(-1, "can't stop processing thread");

    return 0;
}

//********************************************************************************
#pragma mark - private
bool NdnSenderChannel::process()
{
    if (deliverEvent_.Wait(100) == kEventSignaled) {
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