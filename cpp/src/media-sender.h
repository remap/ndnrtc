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

#define NLOG_COMPONENT_NAME "NdnRtcSender"

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"
#include "frame-buffer.h"
#include "segmentizer.h"

namespace ndnrtc
{
    using namespace ndn;
    using namespace ptr_lib;
    
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
        MediaSender(const ParamsStruct &params);
        ~MediaSender();
        
        virtual int init(const shared_ptr<Face> &face,
                         const shared_ptr<Transport> &transport);
        virtual void stop();
        
        unsigned long int getPacketNo() { return packetNo_; }
        
        // encoded packets/second
        double getCurrentPacketRate() {
            return NdnRtcUtils::currentFrequencyMeterValue(packetRateMeter_);
        }

    protected:
        typedef struct _PitEntry {
            int64_t arrivalTimestamp_;
            shared_ptr<const Interest> interest_;
        } PitEntry;
        
        shared_ptr<Transport> ndnTransport_;
        shared_ptr<KeyChain> ndnKeyChain_;
        shared_ptr<Face> ndnFace_;
        shared_ptr<Name> certificateName_;
        shared_ptr<Name> packetPrefix_;
        
        PacketNumber packetNo_ = 0; // sequential packet number
        unsigned int segmentSize_, freshnessInterval_;
        unsigned int packetRateMeter_;
        unsigned int dataRateMeter_;

        // comparator greater is specified in order to use upper_bound method
        // of the map
        std::map<Name, PitEntry> pit_;
        webrtc::CriticalSectionWrapper &pitCs_;
        
        bool isProcessing_ = false;
        webrtc::ThreadWrapper &faceThread_;
        static bool processEventsRoutine(void *obj)
        {
            return ((MediaSender*)obj)->processEvents();
        }
        bool processEvents();

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
        int publishPacket(const PacketData &packetData,
                          shared_ptr<Name> prefix,
                          PacketNumber packetNo,
                          PrefixMetaInfo prefixMeta);
        
        // ndn-cpp callbacks
        virtual void onInterest(const shared_ptr<const Name>& prefix,
                        const shared_ptr<const Interest>& interest,
                        ndn::Transport& transport);
        
        virtual void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix);
        void registerPrefix();
        
        void addToPit(const shared_ptr<const Interest>& interest);
        void lookupPrefixInPit(const Name &prefix,
                               SegmentData::SegmentMetaInfo &metaInfo);
    };
}

#endif /* defined(__ndnrtc__media_sender__) */
