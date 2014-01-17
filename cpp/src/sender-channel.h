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
#include "ndnrtc-utils.h"
#include "audio-channel.h"
#include "statistics.h"

namespace ndnrtc
{
    class NdnMediaChannel : public NdnRtcObject
    {
    public:
        NdnMediaChannel(const ParamsStruct &params,
                        const ParamsStruct &audioParams);
        virtual ~NdnMediaChannel();
        
        // public methods
        virtual int init();
        virtual int startTransmission();
        virtual int stopTransmission();
        
        bool isTransmitting(){ return isTransmitting_; }
        bool isTransmittingAudio() { return isTransmitting_ && audioInitialized_; }
        bool isTransmittingVideo() { return isTransmitting_ && videoInitialized_; }
        
    protected:
        ParamsStruct audioParams_;
        bool videoInitialized_ = false, audioInitialized_ = false;
        bool videoTransmitting_ = false, audioTransmitting_ = false;
        bool isInitialized_ = false, isTransmitting_ = false;
        shared_ptr<ndn::Transport> ndnTransport_, ndnAudioTransport_;
        shared_ptr<Face> ndnFace_, ndnAudioFace_;
        
        static int setupNdnNetwork(const ParamsStruct &params,
                                   const ParamsStruct &defaultParams,
                                   NdnMediaChannel *callbackListener,
                                   shared_ptr<Face> &face,
                                   shared_ptr<ndn::Transport> &transport);
        
        // ndn-cpp callbacks
        virtual void onInterest(const shared_ptr<const Name>& prefix,
                        const shared_ptr<const Interest>& interest,
                        ndn::Transport& transport);
        
        virtual void onRegisterFailed(const ptr_lib::shared_ptr<const Name>&
                                      prefix);
    };
    
    class NdnSenderChannel : public NdnMediaChannel, public IRawFrameConsumer
    {
    public:
        NdnSenderChannel(const ParamsStruct &params,
                         const ParamsStruct &audioParams);
        virtual ~NdnSenderChannel();
        
        // public methods
        int init();
        int startTransmission();
        int stopTransmission();
        
        // interface conformance - IRawFrameConsumer
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
        
        void getChannelStatistics(SenderChannelStatistics &stat);
    private:
        uint64_t lastFrameStamp_ = 0;
        unsigned int frameFreqMeter_;
        
        shared_ptr<CameraCapturer> cc_;
        shared_ptr<NdnRenderer> localRender_;
        shared_ptr<NdnVideoCoder> coder_;
        shared_ptr<NdnVideoSender> sender_;
        shared_ptr<NdnAudioSendChannel> audioSendChannel_;
        
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
        webrtc::ThreadWrapper &processThread_;
        webrtc::EventWrapper &deliverEvent_;
        webrtc::I420VideoFrame deliverFrame_;
        
        // static methods
        static bool processDeliveredFrame(void *obj) {
            return ((NdnSenderChannel*)obj)->process();
        }
        
        // private methods
        bool process();
    };
    
}
#endif /* defined(__ndnrtc__sender_channel__) */
