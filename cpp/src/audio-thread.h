//
//  audio-sender.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__audio_thread__
#define __ndnrtc__audio_thread__

#include "ndnrtc-common.h"
#include "media-thread.h"
#include "ndnrtc-namespace.h"
#include "audio-capturer.h"

namespace ndnrtc
{
    namespace new_api
    {
        class AudioThreadSettings : public MediaThreadSettings
        {
        public:
        };
        
        class AudioThreadStatistics : public MediaThreadStatistics
        {
        public:
            unsigned int nRtpPublished_, nRtcpPublished_;
        };
        
        /**
         * This class is a subclass of MediaThread for audio streams
         */
        class AudioThread : public MediaThread,
                            public IAudioFrameConsumer
        {
        public:
            // construction/desctruction
            AudioThread():MediaThread(){ description_ = "athread"; }
            ~AudioThread(){};
                        
            int init(const AudioThreadSettings& settings);
            
            void
            getStatistics(AudioThreadStatistics& statistics)
            {
                MediaThread::getStatistics(statistics);
                statistics.nRtcpPublished_ = rtcpPacketNo_;
                statistics.nRtpPublished_ = rtpPacketNo_;
            }
            
            // IAudioFrameConsumer interface
            void
            onDeliverRtpFrame(unsigned int len, unsigned char *data)
            { publishRTPAudioPacket(len, data); }
            
            void
            onDeliverRtcpFrame(unsigned int len, unsigned char *data)
            { publishRTCPAudioPacket(len, data); }
            
        private:
            unsigned int rtpPacketNo_, rtcpPacketNo_;
            Name rtpPacketPrefix_;
            Name rtcpPacketPrefix_;
            NdnAudioData adata_;
            
            int publishRTPAudioPacket(unsigned int len, unsigned char *data);
            int publishRTCPAudioPacket(unsigned int len, unsigned char *data);
            
            /**
             * Publishes specified data under the prefix, determined by the
             * parameters provided upon callee creation and by the current packet
             * number, specified in packetNo_ variable of the class.
             */
            int publishPacket(PacketData &packetData,
                              PrefixMetaInfo prefixMeta = (PrefixMetaInfo){0,0,0});
        };
    }
}

#endif /* defined(__ndnrtc__audio_thread__) */
