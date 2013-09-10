//
//  sender-channel.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/29/13
//

#ifndef __ndnrtc__sender_channel__
#define __ndnrtc__sender_channel__

#include "ndnrtc-common.h"
#include "camera-capturer.h"
#include "video-coder.h"
#include "renderer.h"
#include "video-sender.h"

//#include "nsthread-tasks.h"

namespace ndnrtc
{
    
    class NdnSenderChannel : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        // construction/desctruction
        NdnSenderChannel(NdnParams *params);
        virtual ~NdnSenderChannel() { };
        
        // static
        static NdnParams* defaultParams();
        
        // public methods
        bool isTransmitting(){ return isTransmitting_; }
        int init();
        int startTransmission();
        int stopTransmission();
        
        // interface conformance - IRawFrameConsumer
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
        
    private:
//        class MozEncodingTask : public nsRunnable
//        {
//        public:
//            MozEncodingTask(webrtc::I420VideoFrame &videoFrame, NdnVideoCoder *encoder) : encoder_(encoder){ frame_.CopyFrame(videoFrame); }
//            ~MozEncodingTask() { TRACE("task destroyed"); }
//
//            NS_IMETHOD Run() {
//                encoder_->onDeliverFrame(frame_);
//                return NS_OK;
//            }
//
//        private:
//            webrtc::I420VideoFrame frame_;
//            NdnVideoCoder *encoder_;
//        };
//        class MozInitTask : public nsRunnable
//        {
//        public:
//            MozInitTask(NdnSenderChannel *channel) : channel_(channel) {}
//            ~MozInitTask() { TRACE("init task destroyed"); }
//            
//            NS_IMETHOD Run() {
//                channel_->initTest();
//                return NS_OK;
//            }            
//        private:
//            NdnSenderChannel *channel_;
//        };
//                nsCOMPtr<nsIThread> encodingThread_;
        
        void initTest();
        bool isTransmitting_;
        shared_ptr<CameraCapturer> cc_;
        shared_ptr<NdnRenderer> localRender_;
        shared_ptr<NdnVideoCoder> coder_;
        shared_ptr<NdnVideoSender> sender_;
    };
    
}
#endif /* defined(__ndnrtc__sender_channel__) */
