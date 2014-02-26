//
//  frame-data.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__frame_slot__
#define __ndnrtc__frame_slot__

#include "ndnrtc-common.h"
#include "ndnrtc-utils.h"

#define NDNRTC_FRAMEHDR_MRKR 0xf4d4
#define NDNRTC_FRAMEBODY_MRKR 0xfb0d
#define NDNRTC_AUDIOHDR_MRKR 0xa4a4
#define NDNRTC_AUDIOBODY_MRKR 0xabad

namespace ndnrtc {
    /**
     * Base class for storing media data for publishing in ndn
     */
    class PacketData
    {
    public:
        struct PacketMetadata {
            double packetRate_;         // current packet production rate
            PacketNumber sequencePacketNumber_;  // current packet sequence number
        } __attribute__((packed));
        
        PacketData(){}
        virtual ~PacketData() {
            if (data_)
                free(data_);
        }
        
        int
        getLength() { return length_; }
        
        unsigned char*
        getData() { return data_; }
        
    protected:
        unsigned int length_;
        unsigned char *data_ = NULL;
    };
    
    /**
     * Class is used for packaging encoded frame metadata and actual data for
     * transferring over the network.
     * It has also methods for unarchiving this data into an encoded frame.
     */
    class NdnFrameData : public PacketData
    {
    public:
        
        NdnFrameData(webrtc::EncodedImage &frame);
        NdnFrameData(webrtc::EncodedImage &frame, PacketMetadata &metadata);
        ~NdnFrameData();
        
        static int
        unpackFrame(unsigned int length_, const unsigned char *data,
                    webrtc::EncodedImage **frame);
        
        static int
        unpackMetadata(unsigned int length_,
                       const unsigned char *data,
                       PacketMetadata &metadata);
        
        static
        webrtc::VideoFrameType getFrameTypeFromHeader(unsigned int size,
                                                      const unsigned char *headerSegment);
        
        static bool
        isVideoData(unsigned int size,
                    const unsigned char *headerSegment);
        
        static int64_t
        getTimestamp(unsigned int size,
                     const unsigned char *headerSegment);
        
    private:
        struct FrameDataHeader {
            uint16_t                    headerMarker_ = NDNRTC_FRAMEHDR_MRKR;   // 2
            uint32_t                    encodedWidth_;                          // 6
            uint32_t                    encodedHeight_;                         // 10
            uint32_t                    timeStamp_;                             // 14
            int64_t                     capture_time_ms_;                       // 22
            webrtc::VideoFrameType      frameType_;                             // 26
                                                                                //            uint8_t*                    _buffer;
                                                                                //            uint32_t                    _length;
                                                                                //            uint32_t                    _size;
            bool                        completeFrame_;                         // 27
            PacketMetadata               metadata_;                             // 39
            uint16_t                    bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;    // 41
        } __attribute__((packed));
    };
    
    /**
     * Class is used for packaging audio samples actual data and metadata for
     * transferring over the network.
     */
    class NdnAudioData : public PacketData
    {
    public:
        typedef struct _AudioPacket {
            bool isRTCP_;
            int64_t timestamp_;
            unsigned int length_;
            unsigned char *data_;
        } AudioPacket;
        
        NdnAudioData(AudioPacket &packet);
        NdnAudioData(AudioPacket &packet, PacketMetadata &metadata);
        ~NdnAudioData(){}
        
        static int
        unpackAudio(unsigned int len, const unsigned char *data,
                    AudioPacket &packet);
        
        static int
        unpackMetadata(unsigned int len, const unsigned char *data,
                       PacketMetadata &metadata);
        static bool
        isAudioData(unsigned int size,
                    const unsigned char *headerSegment);
        
        static int64_t
        getTimestamp(unsigned int size,
                     const unsigned char *headerSegment);
        
    private:
        struct AudioDataHeader {
            uint16_t        headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
            bool                isRTCP_;
            PacketMetadata      metadata_;
            int64_t             timestamp_;
            uint16_t        bodyMarker_  = NDNRTC_AUDIOBODY_MRKR;
        };
    };
};

#endif /* defined(__ndnrtc__frame_slot__) */
