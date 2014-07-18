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
#include "video-renderer.h"
#include "video-sender.h"
#include "ndnrtc-utils.h"
#include "statistics.h"
#include "audio-capturer.h"
#include "audio-sender.h"

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
        
        // ndn-cpp callbacks
        virtual void onInterest(const shared_ptr<const Name>& prefix,
                                const shared_ptr<const Interest>& interest,
                                ndn::Transport& transport);
        
        virtual void onRegisterFailed(const ptr_lib::shared_ptr<const Name>&
                                      prefix);
        
    protected:
        ParamsStruct audioParams_;
        bool videoInitialized_ = false, audioInitialized_ = false;
        bool videoTransmitting_ = false, audioTransmitting_ = false;
        bool isInitialized_ = false, isTransmitting_ = false;
        shared_ptr<FaceProcessor> videoFaceProcessor_, audioFaceProcessor_;
        shared_ptr<KeyChain> ndnKeyChain_;
    };
    
    class NdnSenderChannel : public NdnMediaChannel,
                            public IRawFrameConsumer,
                            public new_api::IAudioFrameConsumer
    {
    public:
        NdnSenderChannel(const ParamsStruct &params,
                         const ParamsStruct &audioParams,
                         IExternalRenderer *const externalRenderer = nullptr);
        virtual ~NdnSenderChannel();
        
        // public methods
        int
        init();
        
        int
        startTransmission();
        
        int
        stopTransmission();
        
        // interface conformance - IRawFrameConsumer
        void
        onDeliverFrame(webrtc::I420VideoFrame &frame, double timestamp);
        
        // interface conformance - IAudioFrameConsumer
        void
        onDeliverRtpFrame(unsigned int len, unsigned char *data);
        
        void
        onDeliverRtcpFrame(unsigned int len, unsigned char *data);
        
        void
        getChannelStatistics(SenderChannelStatistics &stat);
        
        void
        setLogger(ndnlog::new_api::Logger* logger);
        
    private:
        unsigned int frameFreqMeter_;
        
        shared_ptr<CameraCapturer> cameraCapturer_;
        shared_ptr<IVideoRenderer> localRender_;
        std::vector<shared_ptr<NdnVideoSender>> videoSenders_;
        
        shared_ptr<new_api::AudioCapturer> audioCapturer_;
        std::vector<shared_ptr<NdnAudioSender>> audioSenders_;
        
        webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
        webrtc::ThreadWrapper &processThread_;
        webrtc::EventWrapper &deliverEvent_;
        webrtc::I420VideoFrame deliverFrame_;
        double deliveredTimestamp_;
        
        shared_ptr<FaceProcessor> serviceFaceProcessor_;
        
        // static methods
        static bool
        processDeliveredFrame(void *obj) {
            return ((NdnSenderChannel*)obj)->process();
        }
        
        // private methods
        bool
        process();
        
        // this should reply only to session info interests
        void
        onInterest(const shared_ptr<const Name>& prefix,
                   const shared_ptr<const Interest>& interest,
                   ndn::Transport& transport);
        
        void
        registerSessionInfoPrefix();
        
        void
        publishSessionInfo(ndn::Transport& transport);
    };
    
}
#endif /* defined(__ndnrtc__sender_channel__) */
