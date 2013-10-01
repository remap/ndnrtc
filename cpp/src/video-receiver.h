//
//  video-receiver.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_receiver__
#define __ndnrtc__video_receiver__

#include "ndnrtc-common.h"
#include "video-sender.h"

namespace ndnrtc
{
    class FrameBuffer
    {
    public:
        class Slot;
        
        enum CallResult {
            CallResultError = 0,        // error occurred            
            CallResultOk = 1,           // everything went well
            CallResultNew = 2,          // first segment
            CallResultAssembling = 3,   // frame needs more segments to complete assembling
            CallResultReady = 4,        // frame is ready for decoding
            CallResultLocked = 5,       // slot is locked
            CallResultFull = 6,         // buffer is full
            CallResultNotFound = 7,     // specified frame number was not found
            CallResultBooked = 9        // slot is already booked
        };
        
        // buffer synchronization event structure
        struct Event
        {
            enum EventType {
                EventTypeReady          = 1<<0, // frame is ready for decoding (has been assembled)
                EventTypeFirstSegment   = 1<<1, // first segment arrived for new slot, usually this
                                                // means that segment interest pipelining can start
                EventTypeFreeSlot       = 1<<2, // new slot free for frame assembling
                EventTypeTimeout        = 1<<3, // triggered when timeout happens for any segment interest
                EventTypeError          = 0
            };
            
            static const int AllEventsMask;
            
            EventType type_;            // type of the event occurred
            unsigned int segmentNo_,    // segment number which triggered the event
                            frameNo_;   // frame number
            Slot *slot_;     // corresponding slot pointer
        };
        
        /**
         * Elementary element of a buffer - buffer slot used for assembling frames
         */
        class Slot
        {
        public:
            /**
             * Slot can have different states
             */
            enum State {
                StateFree = 1,  // slot is free for being used
                StateNew = 2,   // slot is being used for assembling, but has not recevied
                                // any data segments yet
                StateAssembling = 3,    // slot is being used for assembling and already has some
                                        // some data segments arrived
                StateReady = 4, // slot assembled all the data and is ready for decoding a frame
                StateLocked = 5 // slot is being used for decoding
            };
            
            Slot(unsigned int slotSize);
            ~Slot();
            
            Slot::State getState() { return state_; }
            unsigned int getFrameNumber() { return frameNumber_; }
            unsigned int assembledSegmentsNumber() { return storedSegments_; }
            unsigned int totalSegmentsNumber() { return segmentsNum_; }
            
            void markFree() { state_ = StateFree; }
            void markNew(unsigned int frameNumber) {
                frameNumber_ = frameNumber;
                state_ = StateNew;
                assembledDataSize_ = 0;
                storedSegments_ = 0;
                segmentsNum_ = 0;
                segmentSize_ = 0;
            }
            void markLocked() {
                stashedState_ = state_;
                state_ = StateLocked;
            }
            void markUnlocked() { state_ = stashedState_; }
            void markAssembling(unsigned int segmentsNum, unsigned int segmentSize) {
                state_ = StateAssembling;
                segmentSize_ = segmentSize;
                segmentsNum_ = segmentsNum;
                
                if (dataLength_ < segmentsNum_*segmentSize_)
                {
                    WARN("slot size is smaller than expected amount of data. enlarging buffer...");
                    dataLength_ = 2*segmentsNum_*segmentSize_;
                    data_ = (unsigned char*)realloc(data_, dataLength_);
                }
            }

            /**
             * Unpacks encoded frame from received data
             * @return  shared pointer to the encoded frame; frame buffer is still
             *          owned by slot.
             */
            shared_ptr<webrtc::EncodedImage> getFrame();
            /**
             * Appends segment to the frame data
             * @param segmentNo Segment number
             * @param dataLength Length of segment data
             * @param data Segment data
             * @details dataLength should normally be equal to segmentSize, except,
             *          probably, for the last segment
             */
            State appendSegment(unsigned int segmentNo, unsigned int dataLength, unsigned char *data);

            static shared_ptr<string> stateToString(FrameBuffer::Slot::State state);
            
        private:
            unsigned int frameNumber_;
            unsigned int dataLength_;
            unsigned int assembledDataSize_, storedSegments_, segmentsNum_;
            unsigned int segmentSize_;
            unsigned char *data_;
            Slot::State state_, stashedState_;
            
            void flushData() { memset(data_, 0, dataLength_); }
        };
        
        FrameBuffer();
        ~FrameBuffer();
        
        int init(unsigned int bufferSize, unsigned int slotSize);
        int flush(); // flushes buffer (except locked frames)
        /**
         * Releases currently locked waiting threads
         */
        void release();
        
        /**
         * *Blocking call.* Calling thread is blocked on this call unless any type of the
         * event specified in events mask has occurred
         * @param eventsMask Mask of event types
         * @param timeout   Time in miliseconds after which call will be returned. If -1 then
         *                  call is blocking unles new event received.
         * @return Occurred event instance
         */
        Event waitForEvents(int &eventsMask, unsigned int timeout = 0xffffffff);
        
        /**
         * Books slot for assembling a frame. Buffer books slot only if there is at 
         * least one free slot.
         * @param frameNumber Number of frame which will be assembled
         * @return  CallResultNew if can slot was booked succesfully
         *          CallResultFull if buffer is full and there are no free slots available
         *          CallResultBooked if slot is already booked
         */
        CallResult bookSlot(unsigned int frameNumber);
        
        /**
         * Marks slot for specified frame as free
         * @param frameNumber Number of frame which slot should be marked as free
         */
        void markSlotFree(unsigned int frameNumber);
        
        /**
         * Locks slot while it is being used by caller (for decoding for example).
         * Lock slots are not writeable and cannot be marked as free unless explicitly unlocked.
         * @param frameNumber Number of frame which slot should be locked
         */
        void lockSlot(unsigned int frameNumber);

        /**
         * Unocks previously locked slot. Does nothing if slot was not locked previously.
         * @param frameNumber Number of frame which slot should be unlocked
         */
        void unlockSlot(unsigned int frameNumber);
        
        /**
         * Marks slot as being assembled - when first segment arrived (in order to initialize slot 
         * buffer properly)
         */
        void markSlotAssembling(unsigned int frameNumber, unsigned int totalSegments, unsigned int segmentSize);
        
        /**
         * Appends segment to the slot. Triggers defferent events according to situation
         * @param frameNumber Number of the frame
         * @param segmentNumber Number of the frame's segment
         */
        CallResult appendSegment(unsigned int frameNumber, unsigned int segmentNumber,
                                 unsigned int dataLength, unsigned char *data);
        
        /**
         * Notifies awaiting thread that the arriaval of a segment of the booked slot was timeouted
         */
        void notifySegmentTimeout(unsigned int frameNumber, unsigned int segmentNumber);
        
        Slot::State getState(unsigned int frameNo);
        
        unsigned int getBufferSize() { return bufferSize_; }
        
    private:
        bool forcedRelease_;
        unsigned int bufferSize_, slotSize_;
        std::vector<shared_ptr<Slot>> freeSlots_;
        std::map<unsigned int, shared_ptr<Slot>> frameSlotMapping_;
//        std::map<int, shared_ptr<Slot>> readySlots_;
//        std::map<Event::EventType, vector<Event>> pendingEvents_;
        
        Event currentEvent_;
        
        webrtc::CriticalSectionWrapper &syncCs_;
        webrtc::EventWrapper &bufferEvent_;
        
        // lock for pending buffer events
        std::list<Event> pendingEvents_;
        webrtc::RWLockWrapper &bufferEventsRWLock_;
        
        void notifyBufferEventOccurred(unsigned int frameNo, unsigned int segmentNo, FrameBuffer::Event::EventType eType, Slot *slot);
        CallResult getFrameSlot(unsigned int frameNo, shared_ptr<Slot> *slot, bool remove = false);
    };
    
    class VideoReceiverParams : public VideoSenderParams {
    public:
        // static
        static VideoReceiverParams* defaultParams()
        {
            VideoReceiverParams *p = static_cast<VideoReceiverParams*>(VideoSenderParams::defaultParams());
            
            p->setIntParam(ParamNameProducerRate, 30);
            
            return p;
        }
        
        // parameters names
        static const std::string ParamNameProducerRate;
        static const std::string ParamNameReceiverId;
        
        // public methods go here
        int getProducerRate() const { return getParamAsInt(ParamNameProducerRate); }
        int getReceiverId() const { return getParamAsInt(ParamNameReceiverId); }
    };
    
    // used by decoder to notify frame provider that frame decodin has finished and
    // it can release frame buffer, cleanup, etc.
//    class IEncodedFrameProvider {
//    public:
//        void onFrameProcessed(Encoded) = 0;
//    }
    
    class NdnVideoReceiver : public NdnRtcObject {
    public:
        NdnVideoReceiver(NdnParams *params);
        ~NdnVideoReceiver();
        
        int init(shared_ptr<Face> face);
        int startFetching();
        int stopFetching();
        
    private:
        enum ReceiverMode {
            ReceiverModeCreated,
            ReceiverModeInit,
            ReceiverModeWaitingFirstSegment,
            ReceiverModeFetch,
            ReceiverModeChase
        };
        
        ReceiverMode mode_;
        
        bool playout_;
        long playoutSleepIntervalUSec_; // 30 fps
        long playoutFrameNo_;
        
        shared_ptr<Face> face_;
        FrameBuffer frameBuffer_;
        IEncodedFrameConsumer *frameConsumer_;
        
        webrtc::ThreadWrapper &playoutThread_, &pipelineThread_, &assemblingThread_;
        
        // static routines for threads to
        static bool playoutThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processPlayout(); }
        static bool pipelineThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processInterests(); }
        static bool assemblingThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processAssembling(); }
        
        // thread main functions (called iteratively)
        bool processPlayout();
        bool processInterests();
        bool processAssembling();
        
        // ndn-lib callbacks
        void onTimeout(const shared_ptr<const Interest>& interest);
        void onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data);
        
        VideoReceiverParams *getParams() { return static_cast<VideoReceiverParams*>(params_); }
        
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
