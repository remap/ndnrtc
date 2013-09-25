//
//  sender-channel.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/29/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
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
static int32_t timestamp = 0;
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
NdnParams* NdnSenderChannel::defaultParams()
{
    NdnParams *p = new NdnParams();
    
    shared_ptr<CameraCapturerParams> cc_sp(CameraCapturerParams::defaultParams());
    shared_ptr<NdnRendererParams> rr_sp(NdnRendererParams::defaultParams());
    shared_ptr<NdnVideoCoderParams> vc_sp(NdnVideoCoderParams::defaultParams());
    shared_ptr<VideoSenderParams> vs_sp(VideoSenderParams::defaultParams());
    
    p->addParams(*cc_sp.get());
    p->addParams(*rr_sp.get());
    p->addParams(*vc_sp.get());
    p->addParams(*vs_sp.get());
    
    return p;
}

//********************************************************************************
#pragma mark - public
int NdnSenderChannel::init()
{
    if (cc_->init() < 0)
        notifyError(-1, "can't intialize camera capturer");
    
    if (localRender_->init() < 0)
        notifyError(-1, "can't intialize renderer");
    
    if (coder_->init() < 0)
        notifyError(-1, "can't intialize video encoder");
    
    //    if (sender_->init() < 0)
    //        return notifyError(-1, "can't intialize camera capturer");
}
int NdnSenderChannel::startTransmission()
{
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