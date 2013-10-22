//
//  media-sender.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__media_sender__
#define __ndnrtc__media_sender__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"

namespace ndnrtc
{
    /**
     * Parameters base abstract class for MediaSender
     */
    class MediaSenderParams : public NdnParams
    {
    public:
        static shared_ptr<MediaSenderParams> defaultParams()
        {
            shared_ptr<MediaSenderParams> p(new MediaSenderParams());
            
            char *hub = (char*)"ndn/ucla.edu/apps";
            char *user = (char*)"testuser";
            
            p->setStringParam(ParamNameNdnHub, hub);
            p->setStringParam(ParamNameProducerId, user);
            p->setIntParam(ParamNameSegmentSize, 1100);
            p->setIntParam(ParamNameFrameFreshnessInterval, 5);
            
            
            return p;
        }

        int getHub(char **hub) const {
            return getParamAsString(ParamNameNdnHub, hub);
        }
        int getProducerId(char **producerId) const {
            return getParamAsString(ParamNameProducerId, producerId);
        }
        int getStreamName(char **streamName) const {
            return getParamAsString(ParamNameStreamName, streamName);
        }
        int getSegmentSize(unsigned int *segmentSize) const {
            return getParamAsInt(ParamNameSegmentSize, (int*)segmentSize);
        }
        int getFreshnessInterval(unsigned int *freshness) const {
            return getParamAsInt(ParamNameFrameFreshnessInterval,
                                 (int*)freshness);
        }
        int getSegmentSize() const {
            return getParamAsInt(ParamNameSegmentSize);
        }
        std::string getUserPrefix() const;
        std::string getStreamPrefix() const;
        std::string getStreamKeyPrefix() const;
        std::string getStreamFramePrefix() const;
    };
    
    /**
     * This is a base class for sending media packets (video frames or audio
     * samples/RTP packets) to ndn network under the following prefix 
     * conventions:
     *      <stream_prefix>/<packet_no>/<segment_no>
     *  where 
     *      stream_prefix   - prefix provided by MediaSenderParams;
     *      packet_no       - sequential number of packet;
     *      segment_no      - segment number (according to NDN conventions).
     * For details on which parameters can be specified, examine 
     * MediaSenderParams class.
     */
    class MediaSender : public NdnRtcObject
    {
    public:
        MediaSender(const NdnParams *params):NdnRtcObject(params), packetNo_(0)
        {}
        ~MediaSender(){};
        
        int init(const shared_ptr<Transport> transport);
        unsigned long int getPacketNo() { return packetNo_; }
        
    protected:
        // private attributes go here
        shared_ptr<Transport> ndnTransport_;
        shared_ptr<KeyChain> ndnKeyChain_;
        shared_ptr<Name> packetPrefix_;
        shared_ptr<Name> certificateName_;
        
        unsigned long int packetNo_; // sequential packet number
        unsigned int segmentSize_, freshnessInterval_;
        
        int publishPacket(unsigned int len, const unsigned char *packetData);
        
        MediaSenderParams *getParams() {
            return static_cast<MediaSenderParams*>(params_);
        }
    };
}

#endif /* defined(__ndnrtc__media_sender__) */
