//
//  media-receiver.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__media_receiver__
#define __ndnrtc__media_receiver__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "media-sender.h"

const double StartSRTT = 20.; // default SRTT value to start from

namespace ndnrtc
{
    extern const double RttFilterAlpha; // SRTT(i+1) = SRTT(i) + alpha*(RTT(i+1) - SRTT(i))
    extern const double RateFilterAlpha;
    extern const double RateDeviation;
    
    class NdnMediaReceiver : public NdnRtcObject {
    public:
        NdnMediaReceiver(const ParamsStruct &params);
        ~NdnMediaReceiver();
        
        virtual int init(shared_ptr<Face> face);
        virtual int startFetching();
        virtual int stopFetching();
        
        double getInterestFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(interestFreqMeter_);
        }
        double getSegmentFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeter_);
        }
        double getDataRate(){
            return  NdnRtcUtils::currentDataRateMeterValue(dataRateMeter_);
        }
        double getFrameFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(frameFrequencyMeter_);
        }
        
        uint64_t getLastRtt() { return rtt_; }
        double getSrtt() { return srtt_; }
        
        unsigned int getNReceived() { return nReceived_; }
        unsigned int getNLost() { return nLost_; }
        unsigned int getNMissed() { return nMissed_; }
        unsigned int getNPlayed() { return nPlayedOut_; }
        unsigned int getRebufferingEvents() { return rebufferingEvent_; }
        
    protected:
        enum ReceiverMode {
            ReceiverModeCreated = 0,
            ReceiverModeInit = 1,
            ReceiverModeFlushed = 2,
            ReceiverModeChase = 3,
            ReceiverModeFetch = 4
        };
        
        ReceiverMode mode_;

        NdnLogger *frameLogger_;
        long pipelinerFrameNo_;
        unsigned int excludeFilter_ = 0; // used for rebufferings
        int pipelinerEventsMask_, interestTimeoutMs_, nTimeoutsKeyFrame_ = 0;
        unsigned int producerSegmentSize_;
        bool hasKeyFrameForGop_ DEPRECATED = true;
        unsigned int gopKeyFrameNo_ DEPRECATED = 0;
        unsigned int fetchAhead_ DEPRECATED = 0;
        Name framesPrefix_;
        shared_ptr<Face> face_;
        double currentProducerRate_;
        
        double srtt_ = StartSRTT;
        uint64_t rtt_ = 0;
        double outstandingMs_;
        unsigned int rebufferingEvent_ = 0;
        unsigned int nReceived_ = 0, nLost_ = 0, nPlayedOut_ = 0, nMissed_ = 0;
        unsigned int interestFreqMeter_, segmentFreqMeter_, dataRateMeter_,frameFrequencyMeter_;
        unsigned int firstFrame_ = 0, lastMissedFrameNo_ = 0;
        
        typedef struct _PendingInterestStruct {
            unsigned int interestID_;
            uint64_t emissionTimestamp_;
        } PendingInterestStruct;
        typedef std::map<const string, PendingInterestStruct> PitMap;
        typedef std::map<unsigned int, string> PitMapUri;
        
        PitMap pendingInterests_;
        PitMapUri pendingInterestsUri_ DEPRECATED;
        
        FrameBuffer frameBuffer_;
        PlayoutBuffer playoutBuffer_;
        
        webrtc::CriticalSectionWrapper &faceCs_;    // needed for synchronous
                                                    // access to the NDN face
                                                    // object
        webrtc::CriticalSectionWrapper &pitCs_;     // PIT table synchronization
        webrtc::ThreadWrapper &pipelineThread_, &assemblingThread_;
        webrtc::EventWrapper &pipelineTimer_;
        
        // static routines for threads
        static bool pipelineThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processInterests();
        }
        static bool assemblingThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processAssembling();
        }
        
        // thread main functions (called iteratively by static routines)
        virtual bool processInterests();
        virtual bool processAssembling();
        
        // ndn-lib callbacks
        virtual void onTimeout(const shared_ptr<const Interest>& interest);
        virtual void onSegmentData(const shared_ptr<const Interest>& interest,
                                   const shared_ptr<Data>& data);
        
        virtual void switchToMode(ReceiverMode mode);
        
        void requestInitialSegment();
        void pipelineInterests(FrameBuffer::Event &event);
        void requestSegment(unsigned int frameNo, unsigned int segmentNo);
        bool isStreamInterest(Name prefix);
        unsigned int getFrameNumber(Name prefix);
        unsigned int getSegmentNumber(Name prefix);
        void expressInterest(Name &prefix);
        void expressInterest(Interest &i);
        void rebuffer();
        
        // derived classes should determine whether a frame with frameNo number
        // is late or not
        virtual bool isLate(unsigned int frameNo);
        void cleanupLateFrame(unsigned int frameNo);
        
        unsigned int getTimeout() const;
        unsigned int getFrameBufferSize() const;
        unsigned int getFrameSlotSize() const;
        
        virtual unsigned int getNextKeyFrameNo(unsigned int frameNo) = 0;
        
        // frame buffer events handlers for pipeliner
        virtual bool onFirstSegmentReceived(FrameBuffer::Event &event);
        virtual bool onSegmentTimeout(FrameBuffer::Event &event);
        virtual bool onFreeSlot(FrameBuffer::Event &event);
        virtual bool onError(FrameBuffer::Event &event);
        
        PendingInterestStruct getPisForInterest(const string &iuri,
                                                bool removeFromPITs = false);
        void clearPITs();
    };
}

#endif /* defined(__ndnrtc__media_receiver__) */
