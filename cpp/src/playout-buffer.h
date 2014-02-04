//
//  playout-buffer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev


#ifndef __ndnrtc__playout_buffer__
#define __ndnrtc__playout_buffer__

#include "ndnrtc-common.h"
#include "frame-buffer.h"
#include "ndnrtc-utils.h"

namespace ndnrtc
{
    typedef std::priority_queue<FrameBuffer::Slot*,
    std::vector<FrameBuffer::Slot*>,
    FrameBuffer::Slot::SlotComparator> FrameJitterBuffer;
    
    const int MinJitterSizeMs = 150;
    const int MaxUnderrunNum = 10;
    const int MaxOutstandingInterests DEPRECATED = 1;
    
    // how fast playout should slow compared to the jitter buffer decrease
    const double PlayoutJitterRatio = 1/3;
    const double ExtraTimePerFrame = 0.3;
    
    const double AmpKeyFrameImportance = 1.5;
    const double AmpMinimalAssembledLevel = 0.7;
    
    class IPlayoutBufferCallback;
    
    class PlayoutBuffer : public LoggerObject
    {
    public:
        enum State {
            StateUnknown = -1,
            StateClear = 0,
            StatePlayback = 1,
            StateBuffering = 2
        };
        
        PlayoutBuffer();
        ~PlayoutBuffer();
        
        int init(FrameBuffer *buffer, double startPacketRate,
                 unsigned int minJitterSize = 1);
        void flush();
        void registerBufferCallback(IPlayoutBufferCallback *callback) {
            callback_ = callback;
        }
        
        /**
         * Returns next frame that should be played out
         */
        virtual FrameBuffer::Slot *acquireNextSlot(bool incCounter = false);
        /**
         * Releases frame slot which was taken for playout (i.e. it should be
         * called when frame has been already decoded and rendered/played)
         *
         * @return Playback time for the released frame in milliseconds, i.e.
         * how long the caller should wait before next acquireNextFrame call.
         * This value is calculated based on the next frame in the jitter
         * buffer. In cases, when it is not possible to calculate this value,
         * it returns -1 (when there are no frames in the buffer - underrun, or
         * next frame's number is greater than then acquired frame's number by
         * more than 1 - missed frame).
         */
        virtual int releaseAcquiredSlot();
        
        int moveTo(unsigned int frameNo){ return 0; }
        unsigned int framePointer() DEPRECATED { return framePointer_; }
        unsigned int getPlayheadPointer() {
            webrtc::CriticalSectionScoped scopedCs(&playoutCs_);
            return playheadPointer_;
        }
        
        unsigned int getJitterSize() { return jitterBuffer_.size(); }
        void setMinJitterSize(unsigned int minJitterSize) {
            webrtc::CriticalSectionScoped scopedCs(&syncCs_);
            minJitterSize_ = minJitterSize;
        }
        unsigned int getMinJitterSize() { return minJitterSize_; }
        
        void setMinJitterSizeMs(unsigned int minJitterSizeMs) {
            webrtc::CriticalSectionScoped scopedCs(&syncCs_);
            minJitterSizeMs_ = minJitterSizeMs;
        }
        unsigned int getJitterSizeMs() { return jitterSizeMs_; }
        
        State getState() { return state_; }
        
    protected:
        bool bufferUnderrun_ = false, missingFrame_ = false;
        State state_ = StateUnknown;
        unsigned int framePointer_, playheadPointer_;
        unsigned int minJitterSize_; // number of frames to keep in buffer before
                                                // they can be played out
        
        unsigned int minJitterSizeMs_; // minimal jitter size in milliseconds
        unsigned int jitterSizeMs_;  // current size of jitter buffer in
                                     // milliseconds
        bool nextFramePresent_ = true;
        int currentPlayoutTimeMs_, adaptedPlayoutTimeMs_;
        uint64_t lastFrameTimestampMs_ = 0;
        
        // Adaptive Media Playback parameters
        int ampExtraTimeMs_ = 0;    // whenever playback is slowed down, this
                                    // variable tracks cummulative amount of
                                    // milliseconds added for each playout
                                    // adaptation
        
        double ampThreshold_ = 0.6;     // size of the jitter buffer which is taken
                                        // as a boundary for enabling of the AMP
                                        // algorithm
        
        int nKeyFrames_ = 0, nDeltaFrames_ = 0; // number of received key and
                                                // delta frames accordingly
        double deltaFrameAvgSize_ = 0.; // average size of delta frames (in segments)
        double keyFrameAvgSize_ = 0.; // average size of key frame (in segments)
        
        FrameBuffer *frameBuffer_;
        // completed frames ready for playout are sorted in ascending order
        FrameJitterBuffer jitterBuffer_;
        IPlayoutBufferCallback *callback_ = NULL;
        
        webrtc::CriticalSectionWrapper &playoutCs_, &syncCs_;
        webrtc::ThreadWrapper &providerThread_;
        
        static bool frameProviderThreadRoutine(void *obj) {
            return ((PlayoutBuffer*)obj)->processFrameProvider();
        }
        bool processFrameProvider();
        
        // Adaptive Media Playout mechanism when jitter size less than
        // ampThreshold_
        void switchToState(State state);
        void adjustPlayoutTiming(FrameBuffer::Slot *slot);
        int calculatePlayoutTime();
        int getAdaptedPlayoutTime(int playoutTimeMs, int jitterSize);
        
        void resetData();
    };
    
    class IPlayoutBufferCallback {
    public:
        virtual void onFrameAddedToJitter(FrameBuffer::Slot *slot) = 0;
        virtual void onPlayheadMoved(unsigned int nextPlaybackFrame) = 0;
        virtual void onBufferStateChanged(PlayoutBuffer::State newState) = 0;
        virtual void onMissedFrame(unsigned int frameNo) = 0;
        virtual void onJitterBufferUnderrun() = 0;
    };
    
    /**
     * Video jitter buffer timing class
     * Provides interface for managing playout timing in separate playout thread
     * Playout thread itratively calls function which extracts frames from the
     * jitter buffer, renders them and sets a timer for the frame playout delay,
     * which is calculated from the timestamps, provided by producer and
     * adjusted by this class in order to accomodate processing delays
     * (extracting frame from the jitter buffer, rendering frame on the canvas,
     * etc.).
     */
    class JitterTiming : public LoggerObject
    {
    public:
        JitterTiming();
        ~JitterTiming(){}
        
        void flush();
        void stop();
        
        /**
         * Should be called in the beginning of the each playout iteration
         * @return current time in microseconds
         */
        int64_t startFramePlayout();
        
        /**
         * Should be called whenever playout time (as provided by producer) is
         * known by the consumer. Usually, jitter buffer provides this value as
         * a result of releaseAcqiuredFrame call.
         * @param framePlayoutTime Playout time meant by producer (difference
         *                         between conqequent frame's timestamps)
         */
        void updatePlayoutTime(int framePlayoutTime);
        
        /**
         * Schedules and runs playout timer for current calculated playout time
         */
        void runPlayoutTimer();
        
    private:
        webrtc::EventWrapper &playoutTimer_;
        
        int framePlayoutTimeMs_ = 0;
        int processingTimeUsec_ = 0;
        int64_t playoutTimestampUsec_ = 0;
        
        void resetData();
    };
}

#endif /* defined(__ndnrtc__playout_buffer__) */
