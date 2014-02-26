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

#include <tr1/unordered_set>

#include "frame-data.h"
#include "ndnrtc-common.h"
#include "ndnrtc-utils.h"

namespace ndnrtc
{
    typedef int BookingId;
    
    /**
     * Frame assembling buffer used for assembling frames and preparing them 
     * for playback. During frame assembling, slot will transit through several
     * states: Free -> New -> Assembling -> Ready -> Locked -> Free. After 
     * decoding, slot should be unlocked and marked as free again for reuse.
     * Before issuing an interest for the frame segment, buffer slot should be 
     * booked for the specified frame number (slot will switch from Free state 
     * to New). Upon receiving first segment of the frame, slot is
     * marked "assembling" (switch to Assembling state). If the frame consists
     * only from 1 segment, frame will switch directly to Ready state, 
     * otherwise it will be ready for decoding upon receiving all the missing
     * segments. Finally, in order to decode a frame, slot should be locked (so 
     * it can't be freed ocasionally, on rebuffering for instance). After 
     * decoding, slot should be unlocked and marked as free (switch back to 
     * Free state).
     * Buffer supports two namespaces for the frames - delta frames
     * and key frames, that means that key numbers from different namespaces 
     * can overlap but mapping from key namespace to delta namespace exists. 
     * Methods, which require specifying namespace has optional parameter
     * useKeyNamespace which is false by default. Another way to specify key 
     * namespace is to use negative key number (internally, key frames numbers 
     * are stored as negative values).
     */
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
                Ready          = 1<<0, // frame is ready for decoding
                                                // (has been assembled)
                FirstSegment   = 1<<1, // first segment arrived for new
                                                // slot, usually this
                // means that segment interest pipelining can start
                FreeSlot       = 1<<2, // new slot free for frame
                                                // assembling
                Timeout        = 1<<3, // triggered when timeout
                                                // happens for any segment
                                                // interest
                Error          = 1<<4  // general error event
            };
            
            static const int AllEventsMask;
            
            PacketNumber packetNo_;
            SegmentNumber segmentNo_;
            EventType type_;            // type of the event occurred
            Slot *slot_;     // corresponding slot pointer
        };
        /**
         * Elementary element of asse,bling buffer - buffer slot used for 
         * assembling frames
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
            
            Slot::State
            getState() const { return state_; }
            
            PacketNumber
            getFrameNumber() const {
                if (isMetaReady())
                {
                    PacketData::PacketMetadata meta;
                    NdnFrameData::unpackMetadata(dataLength_, data_, meta);
                    
                    return meta.sequencePacketNumber_;
                }
                    
                return INT_MAX;
            }
            
            unsigned int
            assembledSegmentsNumber() const { return fetchedSegments_; }
            
            unsigned int
            totalSegmentsNumber() const { return segmentsNum_; }
            
            void
            markFree() { state_ = StateFree; }
            
            void
            markNew(BookingId bookingId, bool isKey) {
                reset();
                
                bookingId_ = bookingId;
                isKeyFrame_ = isKey;
                state_ = StateNew;
            }
            
            void
            markLocked() {
                stashedState_ = state_;
                state_ = StateLocked;
                stateTimestamps_[Slot::StateLocked] = NdnRtcUtils::microsecondTimestamp();
            }
            
            void
            markUnlocked() { state_ = stashedState_; }
            
            void
            markAssembling(unsigned int segmentsNum, unsigned int segmentSize);
            
            BookingId
            getBookingId() { return bookingId_; }
            
            void
            updateBookingId(BookingId newBookId) { bookingId_ = newBookId; }
            
            /**
             * Unpacks encoded frame from received data
             * @return  shared pointer to the encoded frame; frame buffer is 
             *          still owned by slot.
             */
            shared_ptr<webrtc::EncodedImage>
            getFrame();

            /**
             * Unpacks audio frame from received data
             */
            NdnAudioData::AudioPacket
            getAudioFrame();
            
            /**
             * Retrieves packet metadata
             * @return  RESULT_OK if metadata can be retrieved and RESULT_ERR
             *          upon error
             */
            int
            getPacketMetadata(PacketData::PacketMetadata &metadata) const;
            
            /**
             * Returns packet timestamp
             * @return video or audio packet timestamp or RESULT_ERR upon error
             */
            int64_t
            getPacketTimestamp();
            
            /**
             * Appends segment to the frame data
             * @param segmentNo Segment number
             * @param dataLength Length of segment data
             * @param data Segment data
             * @details dataLength should normally be equal to segmentSize, 
             *          except, probably, for the last segment
             */
            State
            appendSegment(SegmentNumber segmentNo, unsigned int dataLength,
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
                        PacketData::PacketMetadata metaA, metaB;
                        int res = RESULT_OK;
                        
                        res = a->getPacketMetadata(metaA);
                        res |= b->getPacketMetadata(metaB);
                        
                        if (RESULT_NOT_OK(res))
                            LOG_NDNERROR("comparing corrupted slots");
                        
                        assert(RESULT_GOOD(res));
                        
                        bool cres = (metaA.sequencePacketNumber_ < metaB.sequencePacketNumber_);
                        return (inverted_)? !cres : cres;
                    }
                    return false;
                }
                
            private:
                bool inverted_;
            };
            
            // indicates, whether slot contains key frame. returns always false
            // for audio frames
            bool
            isKeyFrame() { return isKeyFrame_; };
            
            webrtc::VideoFrameType
            getFrameType() {
                return NdnFrameData::getFrameTypeFromHeader(segmentSize_, data_);
            }
            
            uint64_t
            getAssemblingTimeUsec() {
                return stateTimestamps_[Slot::StateReady] -
                        stateTimestamps_[Slot::StateAssembling];
            }
            
            bool
            isAudioPacket() {
                return NdnAudioData::isAudioData(segmentSize_, data_);
            }
            
            bool
            isVideoPacket(){
                return NdnFrameData::isVideoData(segmentSize_, data_);
            }
            
            std::tr1::unordered_set<SegmentNumber>
            getLateSegments(){
                webrtc::CriticalSectionScoped scopedCs(&cs_);
                return std::tr1::unordered_set<SegmentNumber>(missingSegments_);
            }
            
            int
            getRetransmitRequestedNum() {
                return nRetransmitRequested_;
            }
            
            void
            incRetransmitRequestedNum()
            {
                nRetransmitRequested_++;
            }
            
            unsigned char* getData(){
                return data_;
            }
            
            /**
             * Indicates, whether metadata is already available (i.e. segment 0
             * has been received succesfully)
             */
            bool
            isMetaReady() const
            {
                return (bookingId_ >= 0) &&
                (state_ >= StateAssembling) &&
                (missingSegments_.find(0) == missingSegments_.end());
            }
            
        private:
            bool isKeyFrame_;     // the information about whether slot is
                                  // intended for key or delta frame is known a
                                  // priori - due to separate namespaces for
                                  // these frames types
            BookingId bookingId_; // this is not a real frame number, although
                                  // it can be the same as the real frame
                                  // number, one should not rely on this value
                                  // and use getFrameNumber() call instead
            unsigned int segmentSize_;
            unsigned char *data_;
            unsigned int dataLength_;
            unsigned int assembledDataSize_, fetchedSegments_, segmentsNum_;
            
            // timestamps for state switching
            std::map<Slot::State, int64_t> stateTimestamps_;

            Slot::State state_, stashedState_;
            webrtc::CriticalSectionWrapper &cs_;
            
            int nRetransmitRequested_ = 0;
            std::tr1::unordered_set<SegmentNumber> missingSegments_;
            
            void
            reset();
            
            void
            flushData() { memset(data_, 0, dataLength_); }
        };
        
        FrameBuffer();
        ~FrameBuffer();
        
        int
        init(unsigned int bufferSize, unsigned int slotSize,
             unsigned int segmentSize);
        /**
         * Flushes buffer except currently locked frames
         */
        void
        flush();
        /**
         * Releases currently locked waiting threads
         */
        void
        release();
        /**
         * Indicates, whether buffer is activated.
         * By default, buffer is activated after init call and de-activated 
         * after release call
         */
        bool
        isActivated() { return !forcedRelease_; }
        
        /**
         * *Blocking call* 
         * Calling thread is blocked on this call unless any type of the
         * event specified in events mask has occurred
         * @param eventsMask Mask of event types
         * @param timeout   Time in miliseconds after which call will be returned. If -1 then
         *                  call is blocking unles new event received.
         * @return Occurred event instance
         */
        Event
        waitForEvents(int &eventsMask, unsigned int timeout = 0xffffffff);
        
        /**
         * @deprecated Use newer version instead (with Name argument)
         * Books slot for assembling a frame. Buffer books slot only if there is at
         * least one free slot.
         * @param frameNumber Number of frame which will be assembled
         * @param useKeyNamespace Indicates, whether key namespace should be 
         * used or not
         * @return  CallResultNew if can slot was booked succesfully
         *          CallResultFull if buffer is full and there are no free slots available
         *          CallResultBooked if slot is already booked
         */
//        CallResult
//        bookSlot(PacketNumber frameNumber,
//                 bool useKeyNamespace = false) DEPRECATED;
        
        CallResult
        bookSlot(const Name &prefix, BookingId &bookingId);
        
        /**
         * Marks slot for specified frame as free
         * @param frameNumber Number of frame which slot should be marked as free
         */
        void
        markSlotFree(PacketNumber frameNumber);
        
        /**
         * Marks slot for specified name prefix as free
         * @param prefix Represents issued interest for segment or frame. In 
         * case of multiple consequent calls with different segment prefixes of 
         * the same frame, only 1st call will have effect.
         */
        void
        markSlotFree(const Name &prefix);
        
        /**
         * Locks slot while it is being used by caller (for decoding for example).
         * Lock slots are not writeable and cannot be marked as free unless explicitly unlocked.
         * @param frameNumber Number of frame which slot should be locked
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void
        lockSlot(PacketNumber frameNumber);
        
        /**
         * Unocks previously locked slot. Does nothing if slot was not locked previously.
         * @param frameNumber Number of frame which slot should be unlocked
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void
        unlockSlot(PacketNumber frameNumber);
        
        /**
         * @deprecated Not supported anymore
         * Marks slot as being assembled - when first segment arrived (in order 
         * to initialize slot buffer properly)
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
//        void
//        markSlotAssembling(PacketNumber frameNumber,
//                           unsigned int totalSegments,
//                           unsigned int segmentSize,
//                           bool useKeyNamespace = false) DEPRECATED;
        
        /**
         * @deprecated Use newer version instead (with Data argument)
         * Appends segment to the slot. Triggers defferent events according to
         * the situation
         * @param frameNumber Number of the frame
         * @param segmentNumber Number of the frame's segment
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
//        CallResult
//        appendSegment(PacketNumber frameNumber, SegmentNumber segmentNumber,
//                      unsigned int dataLength, const unsigned char *data,
//                      bool useKeyNamespace = false) DEPRECATED;
        /**
         * Appends data segment to the slot. Triggers defferent events 
         * according to the situation. If slot is in a New state, automatically 
         * marks it as Assembling.
         * @param data Segment data fetched from NDN network
         */
        CallResult
        appendSegment(const Data &data);
        
        /**
         * @deprecated Use newer version instead (with Name argument)
         * Notifies awaiting thread that the arrival of a segment of the booked 
         * slot was timeouted
         * @param frameNumber Number of the frame
         * @param segmentNumber Number of the frame's segment
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
//        void
//        notifySegmentTimeout(PacketNumber frameNumber,
//                             SegmentNumber segmentNumber,
//                             bool useKeyNamespace = false) DEPRECATED;
        
        /**
         * Notifies awaiting thread that the arrival of a segment of the booked
         * slot was timeouted
         * @param preifx Prefix name used for issuing interest for a 
         * segment/frame
         */
        void
        notifySegmentTimeout(Name &prefix);
        
        /**
         * Returns state of slot for the specified booking ID
         * @param booking ID Frame number
         */
        Slot::State
        getState(PacketNumber frameNo);
        
        /**
         * Returns state of slot for the specified prefix
         */
        Slot::State
        getState(const Name &prefix);
        
        /**
         * Buffer size
         */
        unsigned int
        getBufferSize() { return bufferSize_; }
        
        /**
         * Returns number of slots in specified state
         */
        unsigned int
        getStat(Slot::State state);
        
        /**
         * As buffer provides events-per-caller, once they emmited, they can not 
         * be re-issued anymore. In case when calledr does not want to dispatch 
         * buffer event, it should call this method so event can be re-emmited 
         * by buffer.
         */
        void
        reuseEvent(Event &event);
        
        /**
         * @deprecated Use newer version (with Name argument)
         * Retireves current slot from the buffer according to the frame 
         * number provided
         * @param frameNo Frame number corresponding to the slot
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        shared_ptr<Slot>
        getSlot(PacketNumber frameNo, bool useKeyNamespace = false) DEPRECATED
        {
            shared_ptr<Slot> slot(nullptr);
            Name prefix;
            
            getFrameSlot(frameNo, &slot, prefix);
            return slot;
        }
        
        /**
         * Retireves current slot from the buffer according to the prefix
         * provided
         * @param prefix Interest's prefix issued for the slot
         */
        shared_ptr<Slot>
        getSlot(const Name &prefix);
        
        /**
         * Returns booking ID according to the frame number. NOTE: this could 
         * be time-expensive call as all the current slots in the buffer are 
         * traversed and queried for the frame number. Moreover, if multiple 
         * slots have the same number, only ID for the first one will be 
         * returned.
         * @param frameNo Frame number which booking ID should be retrieved
         * @param isKey Indicates, whether the required frame is a key frame
         */
        BookingId getBookingId(PacketNumber frameNo);
        
    private:
        bool forcedRelease_;
        unsigned int bufferSize_, slotSize_, segmentSize_;
        
        unsigned int bookingCounter_ = 0;
        std::tr1::unordered_set<BookingId> activeBookings_;
        std::vector<shared_ptr<Slot>> freeSlots_;
        std::map<Name, shared_ptr<Slot>, greater<Name>> activeSlots_;
//        std::map<PacketNumber, shared_ptr<Slot>> frameSlotMapping_ DEPRECATED;

        // statistics
        std::map<Slot::State, unsigned int> statistics_; // stores number of frames in each state
        
        webrtc::CriticalSectionWrapper &syncCs_;
        webrtc::EventWrapper &bufferEvent_;
        
        // lock object for fetching pending buffer events
        std::list<Event> pendingEvents_;
        webrtc::RWLockWrapper &bufferEventsRWLock_;
        
        void
        notifyBufferEventOccurred(PacketNumber packetNo,
                                  SegmentNumber segmentNo,
                                  FrameBuffer::Event::EventType eType,
                                  Slot *slot);

        /**
         * Returns active slot for specified frame number. If there are several 
         * slots with the same frame number - returns the 1st one.
         * @param frameNo Frame number
         * @param slot Variable for storing found slot
         * @param remove Indicates, should function remove slot if found
         * @return Operation result
         */
        CallResult
        getFrameSlot(PacketNumber frameNo, shared_ptr<Slot> *slot, Name &prefix,
                     bool remove = false);
        
        /**
         * Returns active slot for the specified interest prefix
         * @param prefix Interest prefix used for fetching a slot
         * @return Result of search operation
         */
        CallResult
        getFrameSlot(const Name &prefix, shared_ptr<Slot> *slot,
                     bool remove = false);
        
        /**
         * Returns first matched intial prefix (which has no frame number, 
         * neither segment number).
         * @param prefix Interest prefix
         * @param slot Variable for storing result
         * @param remove Should remove slot from actives and mark as free
         * @return Result of search operation
         */
        CallResult
        getSlotForInitial(const Name &prefix, shared_ptr<Slot> *slot,
                          bool remove = false);
        
        void
        updateStat(Slot::State state, int change);
        
        void
        resetData();
        
        /**
         * By default, this function tries to extract frame number and use it 
         * as a booking ID number. For key frames - frame number is negative. 
         * If no frame number can be retrieved - booking ID is generated.
         * @param prefix Interest prefix
         * @return Booking ID value for provided prefix
         */
        BookingId
        getBookingIdForPrefix(const Name &prefix);
        
        void markSlotFree(const Name &prefix, shared_ptr<Slot> &slot);
        
        void dumpActiveSlots();
    };
}

#endif /* defined(__ndnrtc__frame_buffer__) */
