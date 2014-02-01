//
//  frame-buffer.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__frame_buffer__
#define __ndnrtc__frame_buffer__

#include "ndnrtc-common.h"
#include "ndnrtc-utils.h"

#define NDNRTC_FRAMEHDR_MRKR 0xf4d4
#define NDNRTC_FRAMEBODY_MRKR 0xfb0d
#define NDNRTC_AUDIOHDR_MRKR 0xa4a4
#define NDNRTC_AUDIOBODY_MRKR 0xabad

#define FULL_TIMEOUT_CASE_THRESHOLD 2

namespace ndnrtc
{
    // base class for storing media data for publishing in ndn
    class PacketData
    {
    public:
        struct PacketMetadata {
            double packetRate_;
        };
        
        PacketData(){}
        virtual ~PacketData() {
            if (data_)
                free(data_);
        }
        
        int getLength() { return length_; }
        unsigned char* getData() { return data_; }
        
    protected:
        unsigned int length_;
        unsigned char *data_ = NULL;
    };
    
    /**
     * Class is used for packaging encoded frame metadata and actual data in a 
     * buffer.
     * It has also methods for unarchiving this data into an encoded frame.
     */
    class NdnFrameData : public PacketData
    {
    public:
        
        NdnFrameData(webrtc::EncodedImage &frame);
        NdnFrameData(webrtc::EncodedImage &frame, PacketMetadata &metadata);
        ~NdnFrameData(){}
        
        static int unpackFrame(unsigned int length_, const unsigned char *data,
                               webrtc::EncodedImage **frame);
        static int unpackMetadata(unsigned int length_,
                                  const unsigned char *data,
                                  PacketMetadata &metadata);
        static webrtc::VideoFrameType getFrameTypeFromHeader(unsigned int size,
                                            const unsigned char *headerSegment);
        static bool isVideoData(unsigned int size,
                                const unsigned char *headerSegment);
        static int64_t getTimestamp(unsigned int size,
                                    const unsigned char *headerSegment);
    private:
        struct FrameDataHeader {
            uint32_t                    headerMarker_ = NDNRTC_FRAMEHDR_MRKR;
            uint32_t                    encodedWidth_;
            uint32_t                    encodedHeight_;
            uint32_t                    timeStamp_;
            int64_t                     capture_time_ms_;
            webrtc::VideoFrameType      frameType_;
            //            uint8_t*                    _buffer;
            //            uint32_t                    _length;
            //            uint32_t                    _size;
            bool                        completeFrame_;
            PacketMetadata               metadata_;
            uint32_t                    bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;
        };
    };
    
    class NdnAudioData : public PacketData
    {
    public:
        typedef struct _AudioPacket {
            bool isRTCP_;
            int64_t timestamp_;
            unsigned int length_;
            unsigned char *data_;
        } AudioPacket;
        
        NdnAudioData(AudioPacket &packet);
        NdnAudioData(AudioPacket &packet, PacketMetadata &metadata);
        ~NdnAudioData(){}
        
        static int unpackAudio(unsigned int len, const unsigned char *data,
                               AudioPacket &packet);
        static int unpackMetadata(unsigned int len, const unsigned char *data,
                               PacketMetadata &metadata);
        static bool isAudioData(unsigned int size,
                                const unsigned char *headerSegment);
        static int64_t getTimestamp(unsigned int size,
                                    const unsigned char *headerSegment);
    private:
        struct AudioDataHeader {
            unsigned int        headerMarker_ = NDNRTC_AUDIOHDR_MRKR;
            bool                isRTCP_;
            PacketMetadata      metadata_;
            int64_t             timestamp_;
            unsigned int        bodyMarker_  = NDNRTC_AUDIOBODY_MRKR;
        };
    };
    
    class FrameBuffer : public LoggerObject
    {
    public:
        class Slot;
        
        enum CallResult {
            CallResultError = 0,        // error occurred
            CallResultOk = 1,           // everything went well
            CallResultNew = 2,          // first segment
            CallResultAssembling = 3,   // frame needs more segments to complete
                                        // assembling
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
                EventTypeReady          = 1<<0, // frame is ready for decoding
                                                // (has been assembled)
                EventTypeFirstSegment   = 1<<1, // first segment arrived for new
                                                // slot, usually this
                // means that segment interest pipelining can start
                EventTypeFreeSlot       = 1<<2, // new slot free for frame
                                                // assembling
                EventTypeTimeout        = 1<<3, // triggered when timeout
                                                // happens for any segment
                                                // interest
                EventTypeError          = 1<<4  // general error event
            };
            
            static const int AllEventsMask;
            
            EventType type_;            // type of the event occurred
            unsigned int segmentNo_,    // segment number which triggered the
                                        // event
            frameNo_;   // frame number
            Slot *slot_;     // corresponding slot pointer
        };
        
        /**
         * Elementary element of a buffer - buffer slot used for assembling 
         * frames
         */
        class Slot
        {
        public:
            /**
             * Slot can have different states
             */
            enum State {
                StateFree = 1,  // slot is free for being used
                StateNew = 2,   // slot is being used for assembling, but has
                                // not recevied any data segments yet
                StateAssembling = 3,    // slot is being used for assembling and
                                        // already has some data segments arrived
                StateReady = 4, // slot assembled all the data and is ready for
                                // decoding a frame
                StateLocked = 5 // slot is locked for decoding
            };
            
            Slot(unsigned int slotSize);
            ~Slot();
            
            Slot::State getState() const { return state_; }
            unsigned int getFrameNumber() const { return frameNumber_; }
            unsigned int assembledSegmentsNumber() const { return storedSegments_; }
            unsigned int totalSegmentsNumber() const { return segmentsNum_; }
            
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
            void markAssembling(unsigned int segmentsNum,
                                unsigned int segmentSize) {
                state_ = StateAssembling;
                segmentSize_ = segmentSize;
                segmentsNum_ = segmentsNum;
                startAssemblingTs_ = NdnRtcUtils::microsecondTimestamp();
                
                if (dataLength_ < segmentsNum_*segmentSize_)
                {
                    LOG_WARN("slot size is smaller than expected amount of data. "
                         "enlarging buffer...");
                    dataLength_ = 2*segmentsNum_*segmentSize_;
                    data_ = (unsigned char*)realloc(data_, dataLength_);
                }
            }
            void rename(unsigned int newFrameNumber) {
                frameNumber_ = newFrameNumber;
            }
            
            /**
             * Unpacks encoded frame from received data
             * @return  shared pointer to the encoded frame; frame buffer is 
             *          still owned by slot.
             */
            shared_ptr<webrtc::EncodedImage> getFrame();

            /**
             * Unpacks audio frame from received data
             */
            NdnAudioData::AudioPacket getAudioFrame();
            
            /**
             * Retrieves packet metadata
             * @return  RESULT_OK if metadata can be retrieved and RESULT_ERR
             *          upon error
             */
            int getPacketMetadata(PacketData::PacketMetadata &metadata);
            
            /**
             * Returns packet timestamp
             * @return video or audio packet timestamp or RESULT_ERR upon error
             */
            int64_t getPacketTimestamp();
            
            /**
             * Appends segment to the frame data
             * @param segmentNo Segment number
             * @param dataLength Length of segment data
             * @param data Segment data
             * @details dataLength should normally be equal to segmentSize, 
             *          except, probably, for the last segment
             */
            State appendSegment(unsigned int segmentNo, unsigned int dataLength,
                                const unsigned char *data);
            
            static shared_ptr<string>
                stateToString(FrameBuffer::Slot::State state);
            
            class SlotComparator
            {
            public:
                SlotComparator(bool inverted):inverted_(inverted){}
                
                bool operator() (const Slot *a, const Slot *b) const
                {
                    if (a && b)
                    {
                        bool res = (a->getFrameNumber() < b->getFrameNumber());
                        return (inverted_)? !res : res;
                    }
                    return false;
                }
                
            private:
                bool inverted_;
            };
            
            // indicates, whether slot contains key frame. returns always true
            // for audio frames
            bool isKeyFrame() { return isKeyFrame_; };
            webrtc::VideoFrameType getFrameType() {
                return NdnFrameData::getFrameTypeFromHeader(segmentSize_, data_);
            }
            uint64_t getAssemblingTimeUsec() {
                return assemblingTime_;
            }
            bool isAudioPacket() {
                return NdnAudioData::isAudioData(segmentSize_, data_);
            }
            bool isVideoPacket(){
                return NdnFrameData::isVideoData(segmentSize_, data_);
            }
            
        private:
            bool isKeyFrame_;
            unsigned int frameNumber_;
            unsigned int dataLength_;
            unsigned int assembledDataSize_, storedSegments_, segmentsNum_;
            unsigned int segmentSize_;
            uint64_t startAssemblingTs_;
            uint64_t assemblingTime_;
            unsigned char *data_;
            Slot::State state_, stashedState_;
            
            void flushData() { memset(data_, 0, dataLength_); }
        };
        
        FrameBuffer();
        ~FrameBuffer();
        
        int init(unsigned int bufferSize, unsigned int slotSize);
        void flush(); // flushes buffer (except locked frames)
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
        Event waitForEvents(int &eventsMask,
                            unsigned int timeout = 0xffffffff);
        
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
        void markSlotAssembling(unsigned int frameNumber,
                                unsigned int totalSegments,
                                unsigned int segmentSize);
        
        /**
         * Appends segment to the slot. Triggers defferent events according to situation
         * @param frameNumber Number of the frame
         * @param segmentNumber Number of the frame's segment
         */
        CallResult appendSegment(unsigned int frameNumber,
                                 unsigned int segmentNumber,
                                 unsigned int dataLength,
                                 const unsigned char *data);
        
        /**
         * Notifies awaiting thread that the arrival of a segment of the booked slot was timeouted
         */
        void notifySegmentTimeout(unsigned int frameNumber,
                                  unsigned int segmentNumber);
        
        /**
         * Returns state of slot for the specified frame number
         * @param frameNo Frame number
         */
        Slot::State getState(unsigned int frameNo);
        
        /**
         * Returns encoded frame if it is already assembled. Otherwise - null
         * @param frameNo Frame number
         */
        shared_ptr<webrtc::EncodedImage> getEncodedImage(unsigned int frameNo);
        
        /**
         * Sets new frame number for booked slot
         */
        void renameSlot(unsigned int oldFrameNo, unsigned int newFrameNo);
        
        /**
         * Buffer size
         */
        unsigned int getBufferSize() { return bufferSize_; }
        
        /**
         * Returns number of slots in specified state
         */
        unsigned int getStat(Slot::State state);
        
        /**
         * Returns number of timeouts occurred since last flush
         */
        unsigned int getTimeoutsNumber() { return timeoutSegments_.size(); }
        
        /**
         * As buffer provides events-per-caller, once they emmited, they can not 
         * be re-issued anymore. In case when calledr does not want to dispatch 
         * buffer event, it should call this method so event can be re-emmited 
         * by buffer.
         */
        void reuseEvent(Event &event);
        
        shared_ptr<Slot> getSlot(unsigned int frameNo)
        {
            shared_ptr<Slot> slot(nullptr);
            getFrameSlot(frameNo, &slot);
            return slot;
        }
        
    private:
        bool forcedRelease_, isTrackingFullTimeoutState_ = false,
        shouldCheckFullBufferTimeout_; // this flag indicates state when all
                                       // the slots are started to timeout. this
                                       // can happen when suddenly producer
                                       // becomes unavailable. in case when full
                                       // buffer timeout event occurs, frame
                                       // buffer generates EventTypeTimeout
                                       // event with frame number equals -1
        unsigned int bufferSize_, slotSize_;
        unsigned int lastTimeoutFrameNo_, fullTimeoutCase_ = 0;
        std::vector<shared_ptr<Slot>> freeSlots_;
        std::map<unsigned int, shared_ptr<Slot>> frameSlotMapping_;
        std::map<Slot*, unsigned int> timeoutSlots_ DEPRECATED; // keeps track of currently
                                                     // timeouted slots and number
                                                     // of timeout events occurred
                                                     // to them

        // statistics
        std::map<Slot::State, unsigned int> statistics_; // stores number of frames in each state
        unsigned int nTimeouts_;
        // stores number of timeouts for specific segment. index of this map is
        // made by concatenating frame number and segment number together
        // (assuming that segment number can't be longer than 3-digit number).
        // i.e. map_index = (uint64_t)frame_no * 1000 + segment_no
        std::map<uint64_t, unsigned int> timeoutSegments_;
        
        webrtc::CriticalSectionWrapper &syncCs_;
        webrtc::EventWrapper &bufferEvent_;
        
        // lock object for fetching pending buffer events
        std::list<Event> pendingEvents_;
        webrtc::RWLockWrapper &bufferEventsRWLock_;
        
        void notifyBufferEventOccurred(unsigned int frameNo,
                                       unsigned int segmentNo,
                                       FrameBuffer::Event::EventType eType,
                                       Slot *slot);
        CallResult getFrameSlot(unsigned int frameNo, shared_ptr<Slot> *slot,
                                bool remove = false);
        
        void updateStat(Slot::State state, int change);
    };
}

#endif /* defined(__ndnrtc__frame_buffer__) */
