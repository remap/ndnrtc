//
//  segmentizer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__segmentizer__
#define __ndnrtc__segmentizer__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"
#include "frame-data.h"

#define NDNRTC_SEGHDR_MRKR 0xb4b4
#define NDNRTC_SEGBODY_MRKR 0xb5b5

namespace ndnrtc
{
    /**
     * Slices outgoing data into segments, adds header and publishes on the
     * NDN. Is also registers prefix for incoming interests for frames and
     * tracks them in internal PIT-like table. Whenever data is published,
     * its checked to have appropriate interest in PIT. On positive PIT
     * hits, segments header contains nonce of the pending interest and
     * generation delay.
     */
    class Segmentizer : public NdnRtcObject
    {
    public:
        Segmentizer(const ParamsStruct &params);
        ~Segmentizer();
        
        int init(const shared_ptr<Face> &ndnFace,
                 const shared_ptr<ndn::Transport> &transport);
        
        int publishData(const Name &prefix,
                        const unsigned char *data, unsigned int dataSize,
                        int freshnessSeconds = -1);
        
        class SegmentData : public PacketData
        {
        public:
            typedef struct _SegmentMetaInfo {
                int32_t interestNonce_;
                int64_t interestArrivalMs_;
                int32_t generationDelayMs_;
            } __attribute__((packed)) SegmentMetaInfo;
            
            SegmentData(const unsigned char *data, const unsigned int dataSize);
            SegmentData(const unsigned char *data, const unsigned int dataSize,
                        const SegmentMetaInfo &metaInfo);
            
            void updateMetaInfo(const SegmentMetaInfo &metaInfo);
            
            static int unpackSegmentData(const unsigned char *segmentData,
                                         const unsigned int segmentDataSize,
                                         unsigned char **data,
                                         unsigned int &dataSize,
                                         SegmentMetaInfo &metaInfo);
            
        private:
            typedef struct _SegmentHeader {
                uint16_t headerMarker_ = NDNRTC_SEGHDR_MRKR;
                SegmentMetaInfo metaInfo_;
                uint16_t bodyMarker_ = NDNRTC_SEGBODY_MRKR;
            }  __attribute__((packed)) SegmentHeader;
            
        };
    protected:
        typedef struct _PitEntry {
            int64_t arrivalTimestamp_;
            shared_ptr<const Interest> interest_;
        } PitEntry;
        
        shared_ptr<Transport> ndnTransport_;
        shared_ptr<KeyChain> ndnKeyChain_;
        shared_ptr<Face> ndnFace_;
        shared_ptr<Name> certificateName_;
        
        unsigned int dataRateMeter_;
        
        
        // comparator greater is specified in order to use upper_bound method
        // of the map
        map<Name, PitEntry, greater<Name>> pit_;
        webrtc::CriticalSectionWrapper &syncCs_;
        
        // ndn-cpp callbacks
        void onInterest(const shared_ptr<const Name>& prefix,
                        const shared_ptr<const Interest>& interest,
                        ndn::Transport& transport);
        
        void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix);
        void registerPrefix();
        void lookupPrefixInPit(const Name &prefix,
                               SegmentData::SegmentMetaInfo &metaInfo);
    };
}

#endif /* defined(__ndnrtc__segmentizer__) */
