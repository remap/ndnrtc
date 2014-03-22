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

namespace ndnrtc
{
    class NdnAudioSender : public MediaSender
    {
    public:
        // construction/desctruction
        NdnAudioSender(const ParamsStruct &params):MediaSender(params){}
        ~NdnAudioSender(){};
     
        static int getStreamControlPrefix(const ParamsStruct &params,
                                          string &prefix);
        
        int init(const shared_ptr<Face> &face,
                 const shared_ptr<ndn::Transport> &transport);
        int publishRTPAudioPacket(unsigned int len, unsigned char *data);
        int publishRTCPAudioPacket(unsigned int len, unsigned char *data);
        
        unsigned long int getSampleNo() { return getPacketNo(); }
        
    private:
        unsigned int rtcpPacketNo_;
        shared_ptr<Name> rtcpPacketPrefix_;
        
        /**
         * Publishes specified data under the prefix, determined by the
         * parameters provided upon callee creation and by the current packet
         * number, specified in packetNo_ variable of the class.
         */
        int publishPacket(const PacketData &packetData,
                          PrefixMetaInfo prefixMeta = {0,0,0})
        {
            return MediaSender::publishPacket(packetData, packetPrefix_, packetNo_,
                                 prefixMeta);
        }
    };
}

#endif /* defined(__ndnrtc__audio_sender__) */
