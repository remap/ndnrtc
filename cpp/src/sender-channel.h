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
    class SenderChannelParams : public NdnParams {
    public:
        
        static SenderChannelParams* defaultParams()
        {
            SenderChannelParams *p = new SenderChannelParams();

            char *host =  (char*)"localhost";
            int portnum = 9695;
            
            p->setStringParam(ParamNameConnectHost, host);
            p->setIntParam(ParamNameConnectPort, portnum);
            
            // add params for internal classes
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
        
        // parameters names
        static const std::string ParamNameConnectHost;
        static const std::string ParamNameConnectPort;
        
        int getConnectPort() { return getParamAsInt(ParamNameConnectPort); }
        std::string getConnectHost() { return getParamAsString(ParamNameConnectHost); }
    };
    
    class NdnSenderChannel : public NdnRtcObject, public IRawFrameConsumer
    {
    public:
        // construction/desctruction
        NdnSenderChannel(NdnParams *params);
        virtual ~NdnSenderChannel() { TRACE(""); };
        
        // public methods
        bool isTransmitting(){ return isTransmitting_; }
        int init();
        int startTransmission();
        int stopTransmission();
        
        // interface conformance - IRawFrameConsumer
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
        
        // ndnlib callbacks
        void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, ndn::Transport& transport);
        void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix);
        
    private:
        bool isTransmitting_;
        shared_ptr<ndn::Transport> ndnTransport_;
        shared_ptr<Face> ndnFace_;
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
        SenderChannelParams *getParams() { return static_cast<SenderChannelParams*>(params_); }
    };
    
}
#endif /* defined(__ndnrtc__sender_channel__) */
