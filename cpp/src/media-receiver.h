//
//  media-receiver.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__media_receiver__
#define __ndnrtc__media_receiver__

#include <tr1/unordered_set>

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "media-sender.h"
#include "av-sync.h"

const double StartSRTT = 20.; // default SRTT value to start from

namespace ndnrtc
{
    extern const double RttFilterAlpha; // SRTT(i+1) = SRTT(i) + alpha*(RTT(i+1) - SRTT(i))
    extern const double RateFilterAlpha;
    extern const double RateDeviation;
    extern const unsigned int RebufferThreshold;
    
    class IMediaReceiverCallback;
    
    class NdnMediaReceiver :    public NdnRtcObject,
                                public IPlayoutBufferCallback
    {
    public:
    
        NdnMediaReceiver(const ParamsStruct &params);
        ~NdnMediaReceiver();
        
        virtual int init(shared_ptr<Face> face);
        virtual int startFetching();
        virtual int stopFetching();
        
        void triggerRebuffering();
        
        void registerCallback(IMediaReceiverCallback *callback);
        void deregisterCallback();
        
        double getInterestFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(interestFreqMeter_);
        }
        double getSegmentFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeter_);
        }
        double getDataRate(){
            return  NdnRtcUtils::currentDataRateMeterValue(dataRateMeter_);
        }
        double getActualProducerRate() {
            return currentProducerRate_;
        }
        
        uint64_t getLastRtt() { return rtt_; }
        double getSrtt() { return srtt_; }
        
        unsigned int getNReceived() { return nReceived_; }
        unsigned int getNLost() { return nLost_; }
        unsigned int getNMissed() { return nMissed_; }
        unsigned int getNPlayed() { return nPlayedOut_; }
        unsigned int getRebufferingEvents() { return rebufferingEvent_; }
        unsigned int getNPipelined() { return pipelinerKeyFrameNo_+pipelinerDeltaFrameNo_; }
        unsigned int getJitterOccupancy() { return playoutBuffer_->getJitterSize(); }
        unsigned int getBufferStat(FrameBuffer::Slot::State state) {
            return frameBuffer_.getStat(state);
        }
        
        void setAVSynchronizer(shared_ptr<AudioVideoSynchronizer> &avSync){
            avSync_ = avSync;
            registerCallback(avSync_.get());
        }
        
        // overriden from LoggerObject
        void setLogger(NdnLogger *logger);
        
        // IPlayoutBufferCallback interface
        void onFrameAddedToJitter(FrameBuffer::Slot *slot);
        void onBufferStateChanged(PlayoutBuffer::State newState);
        void onMissedFrame(unsigned int frameNo);
        void onPlayheadMoved(unsigned int nextPlaybackFrame,
                             bool frameWasMissing);
        void onJitterBufferUnderrun();
//        void onFrameReachedDeadline(FrameBuffer::Slot *slot,
//                                   std::tr1::unordered_set<int> &lateSegments);
        
    protected:
        enum ReceiverMode {
            ReceiverModeCreated = 0,
            ReceiverModeInit = 1,
            ReceiverModeFlushed = 2,
            ReceiverModeChase = 3,
            ReceiverModeFetch = 4
        };
        
        ReceiverMode mode_;
        bool keyNamespaceUsed_ = false;
        bool isStartedKeyFetching_ = false;
        
        NdnLogger *frameLogger_;
        PacketNumber pipelinerDeltaFrameNo_, // current pipelining delta frames
                                            // number
                    pipelinerKeyFrameNo_;   // current pipelining key frames
                                            // number
        unsigned int pipelinerBufferSize_ = 0; // pipeliner buffer size -
                                               // number of pending frames
        unsigned int nKeyFramesPending_ = 0; // number of pending key frames
        double deltaSegmentsNum_ = 4,   // average number of segments for
                                            // delta frames
            keySegmentsNum_ = 25;      // average number f segments for key
                                          // frames
        
        PacketNumber firstFrame_ = 0;
        PacketNumber excludeFilter_ = 0, // used for rebufferings
                        keyExcludeFilter_ = 0;
        int pipelinerEventsMask_, interestTimeoutMs_, nTimeoutsKeyFrame_ = 0;
        unsigned int producerSegmentSize_;
        Name streamPrefix_, deltaFramesPrefix_, keyFramesPrefix_;
        shared_ptr<Face> face_;
        double currentProducerRate_;
        
        double srtt_ = StartSRTT;
        uint64_t rtt_ = 0;
//        double outstandingMs_ DEPRECATED;
        unsigned int rebufferingEvent_ = 0;
        unsigned int nReceived_ = 0, nLost_ = 0, nPlayedOut_ = 0, nMissed_ = 0;
        unsigned int nReceivedDelta_ = 0, nReceivedKey_ = 0;
        unsigned int interestFreqMeter_, segmentFreqMeter_, dataRateMeter_;
//        unsigned int lastMissedFrameNo_ DEPRECATED = 0;
        uint64_t playoutLastUpdate_ = 0;
        unsigned int emptyJitterCounter_ = 0;
        bool shouldRequestFrame_ = false;
        
        typedef struct _PendingInterestStruct {
            unsigned int interestID_;
            uint64_t emissionTimestamp_;
        } PendingInterestStruct;
        typedef std::map<const string, PendingInterestStruct> PitMap;
        typedef std::map<unsigned int, string> PitMapUri;
        
        IMediaReceiverCallback *callback_ = nullptr;
        
//        PitMap pendingInterests_ DEPRECATED;
//        PitMapUri pendingInterestsUri_ DEPRECATED;
        
        FrameBuffer frameBuffer_;
        PlayoutBuffer *playoutBuffer_;
        JitterTiming jitterTiming_;
        
        webrtc::CriticalSectionWrapper &faceCs_;    // needed for synchronous
                                                    // access to the NDN face
                                                    // object
        webrtc::CriticalSectionWrapper &pitCs_,
                                       &dataCs_;    // internal data access
                                                    // critical section
        webrtc::ThreadWrapper &pipelineThread_,
                                &bookingThread_,
                                &assemblingThread_,
                                &playoutThread_;
        webrtc::EventWrapper &pipelineTimer_,
                                &needMoreFrames_; // this event is setup each
                                                  // time more frames should be
                                                  // fetched - this happens
                                                  // either when jitter buffer
                                                  // or pipeliner buffer decreases
        webrtc::EventWrapper &modeSwitchedEvent_; // triggered whenever receiver
                                                  // switches to a new mode
        
        // audio/video synchronizer
        shared_ptr<AudioVideoSynchronizer> avSync_;
        
        // static routines for threads
        static bool pipelineThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processInterests();
        }
        static bool bookingThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processBookingSlots();
        }
        static bool assemblingThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processAssembling();
        }
        static bool playoutThreadRoutine(void *obj) {
            return ((NdnMediaReceiver*)obj)->processPlayout();
        }
        
        // thread main functions (called iteratively by static routines)
        virtual bool processInterests();
        // process booking slots and issusing inital interests for the data
        virtual bool processBookingSlots();
        virtual bool processAssembling();
        virtual bool processPlayout();
        
        // ndn-lib callbacks
        virtual void onTimeout(const shared_ptr<const Interest>& interest);
        virtual void onSegmentData(const shared_ptr<const Interest>& interest,
                                   const shared_ptr<Data>& data);
        
        virtual void switchToMode(ReceiverMode mode);
        // must be overriden by subclasses. called from playout thread
        // iteratively each time media frame should be played out
        // @param packetTsLocal Local timestamp of the packet
        virtual void playbackPacket(int64_t packetTsLocal) = 0;
        
        void requestInitialSegment();
        void requestLatestKey();
        void requestSegment(PacketNumber frameNo, unsigned int segmentNo,
                            bool useKeyNamespace = false);
        void pipelineInterests(FrameBuffer::Event &event);
        
        unsigned int getFrameNumber(Name prefix);
        unsigned int getSegmentNumber(Name prefix);
        void expressInterest(Name &prefix);
        void expressInterest(Interest &i);
        virtual void rebuffer(bool shouldNotify = true);
        
        // derived classes should determine whether a frame with frameNo number
        // is late or not
        virtual bool isLate(const Name &prefix, const unsigned char *segmentData,
                            int dataSz);
        bool checkIsLate(const Name &prefix, const unsigned char *segmentData,
                         int dataSz);
        bool checkExclusion(PacketNumber frameNo, bool isKey = false);
        bool isStreamInterest(Name prefix);
        bool isKeyInterest(Name prefix);
        void handleIfInitial(PacketNumber frameNo,
                             const unsigned char *segmentData,
                             bool isKey = false);
        
        void cleanupLateFrame(const Name &prefix);
        
        unsigned int getTimeout() const;
        unsigned int getFrameBufferSize() const;
        unsigned int getFrameSlotSize() const;
        
        unsigned int getJitterBufferSizeMs();
        unsigned int getPipelinerBufferSizeMs();
        // based on the current sizes of jitter and pipeline buffers, determines
        // whether more frames need to be fetched
        bool needMoreFrames();
        virtual bool needMoreKeyFrames();
        
        int getFetchAheadNumber(bool isKeyNamespace);
        
        // frame buffer events handlers for pipeliner
        virtual bool onFirstSegmentReceived(FrameBuffer::Event &event);
        virtual bool onSegmentTimeout(FrameBuffer::Event &event);
        virtual bool onFreeSlot(FrameBuffer::Event &event);
        virtual bool onError(FrameBuffer::Event &event);
        
        PendingInterestStruct getPisForInterest(const string &iuri,
                                                bool removeFromPITs = false) DEPRECATED;
        void clearPITs();
    };
}

#endif /* defined(__ndnrtc__media_receiver__) */
