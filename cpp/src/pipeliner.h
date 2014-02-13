//
//  pipeliner.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author: Peter Gusev
//  Created: 2/11/14
//

#ifndef __ndnrtc__pipeliner__
#define __ndnrtc__pipeliner__

#include "ndnrtc-common.h"
#include "playout-buffer.h"

namespace ndnrtc {
   
    class IPipelinerCallback;
    
    /**
     * Performs interest pipelining logic
     */
    class InterestPipeliner :   public NdnRtcObject,
                                public IPlayoutBufferCallback
    {
    public:
        /**
         * Creates and initializes an instance of interests pipeliner
         * @param frameBuffer Current frame buffer used for assembling
         * frames and booking slots
         * @param callback Pipeliner's callback
         * @param face Currently used face
         * @param fetchAhead Number of interests issued ahead
         * @param useKeyNamespace Determines, whether separate namespace
         * for key frames is used
         */
        InterestPipeliner(ParamsStruct &params,
                          FrameBuffer *frameBuffer,
                          IPipelinerCallback *callback);
        ~InterestPipeliner();
        
        /**
         * Starts pipeliner.
         * Pipeliner always start its processing by expressing initial interest 
         * for rightmost child in the frames namespace (using key namespace, if 
         * specified)
         * @param useKeyNamespace Specifies, whether key namespace should be 
         * used to organise pipeline logic or not
         */
        int start(bool useKeyNamespace = false);
        /**
         * Stops pipeliner.
         * Any face callbacks received after this call will not be forwarded to 
         * the pipeliner's callback
         */
        int stop();
        
        /**
         * Current buffer size
         * @return pipeliner buffer size in interests number
         */
        int getPipelinerBufferSize(){
            webrtc::CriticalSectionWrapper scopedCs(&generalCs_);
            return pendingInterests_.size();
        }
        
        /**
         * Current buffer size estimation (in ms).
         * As there is no information of exact duration of frames requested
         * (interests are issued for future frames) this call returns
         * duration estimation based on recent producer packet rate
         * @param producerRate Current producer's packet generation rate
         * @return Pipeliner buffer size estimation in milliseconds
         */
        double getPipelinerBufferEstimationMs(double producerRate){
            webrtc::CriticalSectionWrapper scopedCs(&generalCs_);
            return NdnRtcUtils::toTimeMs(pendingInterests_.size(), producerRate);
        }
        
        /**
         * Sets current face (thread-safe wrapper).
         */
        void setFace(shared_ptr<FaceWrapper> &face){ face_ = face; }
        
        /**
         * Initiates exression of interest for the rightmost child (optionaly 
         * in key namespace) with optional exclusion filter
         * @param useKeyNamespace Indicates, whether key namespace should be 
         * used for the inetest
         * @param exclusionFilter Minimal frame number
         */
        void requestLatestSegment(bool useKeyNamespace = false,
                                  int exclusionFilter = -1);
        /**
         * Initiates expression of interest for specified frame and segment
         * (optionaly, in key frame namespace)
         * @param frameNo Frame number
         * @param segmentNo Segment number
         * @param isKey Indicates, whether interest should be expressed in key 
         * frame namespace or not
         */
        void requestSegment(unsigned int frameNo, unsigned int segmentNo,
                            bool isKey = false);
        
        // IPlayoutCallback interface
        void onFrameAddedToJitter(FrameBuffer::Slot *slot);
        void onBufferStateChanged(PlayoutBuffer::State newState);
        void onMissedFrame(unsigned int frameNo);
        void onPlayheadMoved(unsigned int nextPlaybackFrame,
                             bool frameWasMissing);
        void onJitterBufferUnderrun();
        void onFrameReachedDeadline(FrameBuffer::Slot *slot,
                                    std::tr1::unordered_set<int> &lateSegments);
        
        double getInterestFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(interestFreqMeter_);
        }
        double getSegmentFrequency() {
            return NdnRtcUtils::currentFrequencyMeterValue(segmentFreqMeter_);
        }
        double getDataRate(){
            return  NdnRtcUtils::currentDataRateMeterValue(dataRateMeter_);
        }
        
        typedef enum _Mode {
            Created = 0,
            Init = 1,
            Flushed = 2,
            Chase = 3,
            Fetch = 4
        } Mode;
        Mode getCurrentMode() {
            webrtc::CriticalSectionScoped(generalCs_);
            return mode_;
        }
        
    private:
        bool isRunning_ = false, useKeyNamespace_;
        Mode mode_ = Created;
        
        FrameBuffer *buffer_ = nullptr;
        IPipelinerCallback *callback_ = nullptr;
        
        shared_ptr<Face> face_;
        Name deltaPrefix_, keyPrefix_, streamPrefix_;
        
        webrtc::CriticalSectionWrapper &generalCs_;
        webrtc::ThreadWrapper &pipelineThread_, &frameBookingThread_,
                                &assemblerThread_;
        webrtc::EventWrapper &needMoreFrames_;
        
        int64_t exclude_ = -1;
        int64_t deltaFrameNo_ = 0, keyFrameNo_ = 0;
        unsigned int fetchAhead_ = 0;
        
        typedef struct _PendingInterestStruct {
            unsigned int interestID_;
            uint64_t emissionTimestamp_;
        } PendingInterestStruct;
        typedef std::map<const string, PendingInterestStruct> PitMap;
        typedef std::map<unsigned int, string> PitMapUri;
        
        PitMap pendingInterests_ DEPRECATED;
        PitMapUri pendingInterestsUri_ DEPRECATED;
        
        unsigned int interestFreqMeter_, segmentFreqMeter_, dataRateMeter_;
        
        static bool bufferProcessingRoutine(void *obj)
        {
            return ((InterestPipeliner*)obj)->processBufferEvents();
        }
        bool processBufferEvents();
        
        static bool interestPipelineRoutine(void *obj)
        {
            return ((InterestPipeliner*)obj)->pipelineInterests();
        }
        bool pipelineInterests();
        
        static bool faceProcessingRoutine(void *obj)
        {
            return ((InterestPipeliner*)obj)->processFaceEvents();
        }
        bool processFaceEvents();
        
        // ndn-lib callbacks
        virtual void onTimeout(const shared_ptr<const Interest>& interest);
        virtual void onSegmentData(const shared_ptr<const Interest>& interest,
                                   const shared_ptr<Data>& data);
        
        // frame buffer events handlers for pipeliner
        virtual bool onFirstSegmentReceived(FrameBuffer::Event &event);
        virtual bool onSegmentTimeout(FrameBuffer::Event &event);
        virtual bool onFreeSlot(FrameBuffer::Event &event);
        virtual bool onError(FrameBuffer::Event &event);
        
        static string modeToString(Mode mode);
        void switchToMode(Mode newMode);
        
        void init();
        void resetData();
        
        bool isStreamInterest(Name prefix);
        bool isFramesInterest(Name prefix, bool checkForKeyNamespace);
        
        unsigned int getFrameNumber(Name prefix);
        unsigned int getSegmentNumber(Name prefix);
        void expressInterest(Name &prefix);
        void expressInterest(Interest &i);
        
        PendingInterestStruct getPisForInterest(const string &iuri,
                                                bool removeFromPITs = false) DEPRECATED;
        void clearPITs();
        
        bool needMoreFrames(){
            bool jitterBufferSmall = false;
            bool pipelineBufferSmall = getPipelinerBufferSizeMs() < params_.jitterSize;
            
            TRACE("need more frames? jitter small: %s, pipeline small: %s "
                  "underrun: %s",
                  (jitterBufferSmall)?"YES":"NO",
                  (pipelineBufferSmall)?"YES":"NO",
                  (shouldRequestFrame_)?"YES":"NO");
            
            return (jitterBufferSmall ||
                    pipelineBufferSmall ||
                    shouldRequestFrame_) &&
            mode_ != ReceiverModeChase;
        }
    };
    
    /**
     * Pipeliner callback interface class
     */
    class IPipelinerCallback
    {
    public:
        /**
         * Called each time pipeliner switches from one mode to another
         * @param mode New mode of the pipeliner
         */
        virtual void onModeSwitched(InterestPipeliner::Mode mode) = 0;
        
        /**
         * Called when new data (frame segment) was recevied from the face
         * @param frameNo Frame number
         * @param segmentNo Segment number
         * interest. If so, frameNo and segmenNo will both equal 0.
         * @param error Indicates, whether there was an error in receiving data
         * (callback from wrong face). If so, frameNo, segmentNo and dataSize
         * will equal -1 and data will be null.
         * @param data Received data
         * @param dataSize Received data size
         */
        virtual void onData(int frameNo, int segmentNo, int finalSegment,
                            bool error, const char *data,
                            unsigned int dataSize) = 0;
        
        /**
         * Called on interests' timeouts
         * @param frameNo Frame number for which interest was expressed
         * @param segmentNo Segment number for which interest was expressed
         * @param isInitial Determines, whether the timeouted interest was
         * expressed for initiation. If it's true, frameNo and segmentNo will
         * both equal 0
         * @param error Determines whether pipeliner received timeout callback
         * on the unknown interest. If it's true, frameNo and segmentNo will
         * both equal -1
         */
        virtual void onInterestTimeout(int frameNo, int segmentNo,
                                       bool isInitial, bool error);
    };
}

#endif /* defined(__ndnrtc__pipeliner__) */
