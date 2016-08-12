//
//  webrtc-audio-channel.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/14/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __ndnrtc__webrtc_audio_channel__
#define __ndnrtc__webrtc_audio_channel__

namespace webrtc {
    class VoEBase;
    class VoENetwork;
    class VoECodec;
    struct CodecInst;
}

namespace ndnrtc {
    /**
     * This class is a wrapper around WebRTC's audio channel API.
     * One should derive from this class in order to register for 
     * RTP/RTCP audio packet callbacks
     * @see AudioCapturer
     */
     class WebrtcAudioChannel
     {
     public:
        enum class Codec {
            Opus,   // HD, ~400 Kbit/s
            G722    // SD, ~200 Kbit/s
        };

        WebrtcAudioChannel(const Codec& codec);
        virtual ~WebrtcAudioChannel();

        static Codec fromString(std::string codec);
    protected:
        int webrtcChannelId_;
        webrtc::VoEBase* voeBase_;
        webrtc::VoENetwork* voeNetwork_;
        webrtc::VoECodec* voeCodec_;

        webrtc::CodecInst instFromCodec(const Codec& c);
    };
}

#endif /* defined(__ndnrtc__webrtc_audio_channel__) */
