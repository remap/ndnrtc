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
        MediaSender(){}
        MediaSender(const ParamsStruct &params) :
        NdnRtcObject(params),
        packetNo_(0)
        {}
        ~MediaSender(){};
        
        static int getUserPrefix(const ParamsStruct &params, string &prefix);
        static int getStreamPrefix(const ParamsStruct &params, string &prefix);
        static int getStreamFramePrefix(const ParamsStruct &params,
                                        string &prefix);
        static int getStreamKeyPrefix(const ParamsStruct &params,
                                      string &prefix);
        
        virtual int init(const shared_ptr<Transport> transport);
        unsigned long int getPacketNo() { return packetNo_; }
        
    protected:
        // private attributes go here
        shared_ptr<Transport> ndnTransport_;
        shared_ptr<KeyChain> ndnKeyChain_;
        shared_ptr<Name> packetPrefix_;
        shared_ptr<Name> certificateName_;
        
        unsigned long int packetNo_ = 0; // sequential packet number
        unsigned int segmentSize_, freshnessInterval_;

        /**
         * Publishes specified data in the ndn network under specified prefix by
         * appending packet number to it. Packet number IS NOT incremented by 
         * default, instead it should be incremented by derived classes upon 
         * neccessity. Big data blocks will be splitted by segments and 
         * published under the "<prefix>/<packetNo>/<segment>" prefix. Each 
         * segment has the number of the last segment in its' FinalBlockId 
         * field, so upon retrieving any segment, consumer can evaluate total 
         * number of segments for this data.
         * @param len Length of the data in bytes
         * @param packetData Pointer to the data being published
         * @param prefix NDN name prefix under which data will be published
         * @return Number of segments used for publishing data or RESULT_FAIL on 
         *          error
         */
        int publishPacket(unsigned int len,
                          const unsigned char *packetData,
                          shared_ptr<Name> prefix,
                          unsigned int packetNo);
        /**
         * Publishes specified data under the prefix, determined by the
         * parameters provided upon callee creation and for the current packet 
         * number, specified in packetNo_ variable of the class.
         */
        int publishPacket(unsigned int len,
                          const unsigned char *packetData)
        {
            return publishPacket(len, packetData, packetPrefix_, packetNo_);
        }
    };
}

#endif /* defined(__ndnrtc__media_sender__) */
