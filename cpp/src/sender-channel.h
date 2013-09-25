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

namespace ndnrtc
{
    
    class NdnSenderChannel : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        // construction/desctruction
        NdnSenderChannel(NdnParams *params);
        virtual ~NdnSenderChannel() { TRACE(""); };
        
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
        bool isTransmitting_;
        shared_ptr<CameraCapturer> cc_;
        shared_ptr<NdnRenderer> localRender_;
        shared_ptr<NdnVideoCoder> coder_;
        shared_ptr<NdnVideoSender> sender_;
        
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
        webrtc::ThreadWrapper &processThread_;
        webrtc::EventWrapper &deliverEvent_;
        webrtc::I420VideoFrame deliverFrame_;
        
        // static methods
        static bool processDeliveredFrame(void *obj) { return ((NdnSenderChannel*)obj)->process(); }
        
        // private methods
        bool process();
    };
    
}
#endif /* defined(__ndnrtc__sender_channel__) */
