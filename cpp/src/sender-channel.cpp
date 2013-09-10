//
//  sender-channel.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/29/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "sender-channel.h"

using namespace ndnrtc;

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
sender_(new NdnVideoSender(params))
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
//    timestamp += 90000/30.;
//
//    webrtc::I420VideoFrame *frame_copy = new webrtc::I420VideoFrame();
//    frame_copy->CopyFrame(frame);
//    frame_copy->set_timestamp(timestamp);
    
    // pass frame for rendering and encoding
    localRender_->onDeliverFrame(frame);
    
//    nsCOMPtr<nsRunnable> encoderTask = new MozEncodingTask(frame, coder_.get());
//    NS_DispatchToMainThread(encoderTask);
//    encodingThread_->Dispatch(encoderTask, nsIThread::DISPATCH_NORMAL);
    
    coder_->onDeliverFrame(frame);
};

//********************************************************************************
#pragma mark - public
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
int NdnSenderChannel::init()
{
//    nsresult rv;
//    nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);    
//    
//    if (!encodingThread_)
//    {        
//        if (rv != NS_OK)
//            return notifyError(-1, "can't access mozilla thread manager");
//        
//        rv = tm->NewThread(0, 0, getter_AddRefs(encodingThread_));
//        
//        if (rv != NS_OK)
//            return notifyError(-1, "can't create encdoing thread");
//    }
    
//    nsCOMPtr<nsIThread> initThread;
//    rv = tm->NewThread(0, 0, getter_AddRefs(initThread));
//    
//    if (rv != NS_OK)
//        return notifyError(-1, "can't create init thread");

    
//    nsCOMPtr<nsRunnable> initEvent = new MozInitTask(this);
//    encodingThread_->Dispatch(initEvent, nsIThread::DISPATCH_NORMAL);

    initTest();
    
    return 0;
}
void NdnSenderChannel::initTest()
{
    if (cc_->init() < 0)
        notifyError(-1, "can't intialize camera capturer");
    
    if (localRender_->init() < 0)
        notifyError(-1, "can't intialize renderer");
    
    if (coder_->init() < 0)
        notifyError(-1, "can't intialize video encoder");
    
//    startTransmission();
    //    if (sender_->init() < 0)
    //        return notifyError(-1, "can't intialize camera capturer");
}
int NdnSenderChannel::startTransmission()
{
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
    
    isTransmitting_ = false;
    return 0;
}