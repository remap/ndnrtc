//
//  realtime-arc.h
//  ndnrtc
//
//  Copyright 2014 Panasonic corporation
//  For licensing details see the LICENSE file.
//
//  Authors:  Peter Gusev, Takahiro Yoneda
//

#ifndef ndnrtc_realtime_adaptive_rate_control_h
#define ndnrtc_realtime_adaptive_rate_control_h

#ifdef USE_ARC

#define SSTBL_MAX 10
#define MIN_RTT_EXPIRE_MS 10000
#define X_BYTE 1024

//#include <boost/multi_index_container.hpp>
//#include <boost/multi_index/tag.hpp>
//#include <boost/multi_index/ordered_index.hpp>
//#include <boost/multi_index/member.hpp>

#include "./rate-adaptation-module.h"

namespace ndnrtc {
    
    class RealTimeAdaptiveRateControl : public IRateAdaptationModule {
    public:
        RealTimeAdaptiveRateControl ();
        ~RealTimeAdaptiveRateControl ();
        virtual int initialize(const CodecMode& codecMode,
                               uint32_t nStreams,
                               StreamEntry* streamArray);
        virtual void interestExpressed(const std::string &name,
                                       unsigned int streamId);
        virtual void interestTimeout(const std::string &name,
                                     unsigned int streamId);
        virtual void dataReceived(const std::string &name,
                                  unsigned int streamId,
                                  unsigned int dataSize,
                                  double rttMs,
                                  unsigned int nRtx);
        virtual void getInterestRate(int64_t timestampMs,
                                     double& interestRate,
                                     unsigned int& streamId);
        
    protected:
        void getStreamByPps (double pps,
                             unsigned int& id,
                             double& bitrate,
                             double& parity_ratio);
        double getStreamBitrate (unsigned int id);
        double getStreamParityRatio (unsigned int id);
        double convertPpsToBps (double pps);
        double convertBpsToPps (double bps);
        
        CodecMode codecMode_;
        unsigned int numStream_;
        StreamEntry switchStreamTable[SSTBL_MAX];
        unsigned int currentStreamId_;
        double interestPps_;
        
    private:
        uint32_t diffSeq (uint32_t a, uint32_t b);
        int64_t diffTimestamp (int64_t a, int64_t b);
        int64_t addTimestamp (int64_t a, int64_t b);
        
        uint32_t indexSeq_, lastRcvSeq_, lastEstSeq_;
        double avgDataSize_;
        double prevAvgRttMs_, minRttMs_, minRttMsNext_, offsetJitterMs_;
        uint64_t updateMinRttTs_;
        bool congestionSign_;
        /*
        struct InterestHistry {
            InterestHistry (const uint32_t _seq, const std::string& _name, const unsigned int _sid) :
            seq (_seq), name (_name), sid (_sid), rtt (0), rx_count (0), retx_count (0), timeout_count (0) { }
            uint32_t seq;
            std::string name;
            unsigned int sid;
            double rtt;
            unsigned int rx_count;
            unsigned int retx_count;
            unsigned int timeout_count;
            uint32_t GetSeq () const { return seq; }
            std::string GetName () const { return name; }
            unsigned int GetSid () const { return sid; }
            double GetRtt () const { return rtt; }
            unsigned int GetRxCount () const { return rx_count; }
            unsigned int GetRetxCount () const { return retx_count; }
            unsigned int GetTimeoutCount () const { return timeout_count; }
        };
        
        class i_seq { };
        class i_name { };
        
        typedef boost::multi_index::multi_index_container<
		    InterestHistry,
		    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
        boost::multi_index::tag<i_seq>,
        boost::multi_index::member<InterestHistry, uint32_t, &InterestHistry::seq>
        >,
        boost::multi_index::ordered_unique<
        boost::multi_index::tag<i_name>,
        boost::multi_index::member<InterestHistry, std::string, &InterestHistry::name>
        >
		    >
        > InterestHistryContainer;
        
        typedef InterestHistryContainer::index<i_name>::type name_map;
        typedef InterestHistryContainer::index<i_seq>::type seq_map;  
        InterestHistryContainer InterestHistries_;
        */
    };
}
#endif

#endif
