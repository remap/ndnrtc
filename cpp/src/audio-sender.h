//
//  audio-sender.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__audio_sender__
#define __ndnrtc__audio_sender__

#include "ndnrtc-common.h"
#include "media-sender.h"
#include "ndnrtc-namespace.h"
#include "audio-capturer.h"

namespace ndnrtc
{
    class NdnAudioSender : public MediaSender,
    public new_api::IAudioFrameConsumer
    {
    public:
        // construction/desctruction
        NdnAudioSender(const ParamsStruct &params):MediaSender(params)
        { description_ = "asender"; }
        ~NdnAudioSender(){};
     
        static int getStreamControlPrefix(const ParamsStruct &params,
                                          std::string &prefix);
        
        int init(const shared_ptr<FaceProcessor>& faceProcessor,
                 const shared_ptr<KeyChain>& ndnKeyChain);
        
        unsigned long int getSampleNo() { return getPacketNo(); }
        
        void
        onDeliverRtpFrame(unsigned int len, unsigned char *data)
        { publishRTPAudioPacket(len, data); }
        
        void
        onDeliverRtcpFrame(unsigned int len, unsigned char *data)
        { publishRTCPAudioPacket(len, data); }
        
    private:
        unsigned int rtcpPacketNo_;
        shared_ptr<Name> rtcpPacketPrefix_;
        
        int publishRTPAudioPacket(unsigned int len, unsigned char *data);
        int publishRTCPAudioPacket(unsigned int len, unsigned char *data);
        
        /**
         * Publishes specified data under the prefix, determined by the
         * parameters provided upon callee creation and by the current packet
         * number, specified in packetNo_ variable of the class.
         */
        int publishPacket(PacketData &packetData,
                          PrefixMetaInfo prefixMeta = {0,0,0});
    };
}

#endif /* defined(__ndnrtc__audio_sender__) */
