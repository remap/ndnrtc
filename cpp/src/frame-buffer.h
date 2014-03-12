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

#include "fetch-channel.h"

namespace ndnrtc
{
    // namespace for new API
    namespace new_api
    {
        class FrameBuffer : public ndnlog::new_api::ILoggingObject
        {
        public:
            /**
             * Buffer state
             */
            enum State {
                Invalid = 0,
                Valid = 1
            };
            
            /**
             * Buffer slot represents frame/packet which is being assembled
             */
            class Slot {
            public:
                /**
                 * Slot (frame/packet data) is being assembled from several ndn
                 * segments
                 */
                class Segment {
                public:
                    enum State {
                        StateFetched = 1<<0,   // segment has been fetched already
                        StatePending = 1<<1,   // segment awaits it's interest to
                                            // be answered
                        StateMissing = 1<<2,   // segment was timed out or
                                            // interests has not been issued yet
                        StateNotUsed = 1<<3    // segment is no used in frame
                                            // assembling
                    };
                    
                    Segment();
                    ~Segment();
                    
                    /**
                     * Discards segment by swithcing it to NotUsed state and
                     * reseting all the attributes
                     */
                    void discard();
                    
                    /**
                     * Moves segment into Pending state and updaets following
                     * attributes:
                     * - requestTimeUsec
                     * - interestName
                     * - interestNonce
                     * - reqCounter
                     */
                    void interestIssued(const uint32_t& nonceValue);
                    
                    /**
                     * Moves segment into Missing state if it was in Pending
                     * state before
                     */
                    void markMissed();
                    
                    /**
                     * Moves segment into Fetched state and updates following
                     * attributes:
                     * - dataName
                     * - dataNonce
                     * - generationDelay
                     * - arrivalTimeUsec
                     */
                    void
                    dataArrived(const SegmentData::SegmentMetaInfo& segmentMeta);
                    
                    /**
                     * Returns true if the interest issued for a segment was
                     * answered by a producer
                     */
                    bool
                    isOriginal();
                    
                    void
                    setPayloadSize(unsigned int payloadSize)
                    { payloadSize_ = payloadSize; }
                    
                    unsigned int
                    getPayloadSize() const { return payloadSize_; }
                    
                    unsigned char*
                    getDataPtr() const { return dataPtr_; }
                    
                    void
                    setDataPtr(const unsigned char* dataPtr)
                    { dataPtr_ = const_cast<unsigned char*>(dataPtr); }

                    void
                    setNumber(SegmentNumber number) { segmentNumber_ = number; }
                    
                    SegmentNumber
                    getNumber() const { return segmentNumber_; }
                    
                    SegmentData::SegmentMetaInfo getMetadata() const;
                    
                    State
                    getState() const { return state_; };
                    
                    int64_t
                    getArrivalDelay()
                    { return (arrivalTimeUsec_-requestTimeUsec_); }
                    
                    void
                    setPrefix(const Name& prefix)
                    { prefix_ = prefix; }
                    
                    const Name&
                    getPrefix() { return prefix_; };
                    
                    static std::string
                    stateToString(State s)
                    {
                        switch (s)
                        {
                            case StateFetched: return "Fetched"; break;
                            case StateMissing: return "Missing"; break;
                            case StatePending: return "Pending"; break;
                            case StateNotUsed: return "Not used"; break;
                            default: return "Unknown"; break;
                        }
                    }
                protected:
                    SegmentNumber segmentNumber_;
                    Name prefix_;
                    unsigned int payloadSize_;  // size of actual data payload
                                                // (without segment header)
                    unsigned char* dataPtr_;    // pointer to the payload data
                    State state_;
                    int64_t requestTimeUsec_, // local timestamp when the interest
                                              // for this segment was issued
                    arrivalTimeUsec_, // local timestamp when data for this
                                      // segment has arrived
                    consumeTimeMs_;   // remote timestamp (milliseconds)
                                      // when the interest, issued for
                                      // this segment, has been consumed
                                      // by a producer. could be 0 if
                                      // interest was not answered by
                                      // producer directly (cached on
                                      // the network)
                    int reqCounter_; // indicates, how many times segment was
                                     // requested
                    uint32_t dataNonce_; // nonce value provided with the
                                         // segment's meta data
                    uint32_t interestNonce_; // nonce used with issuing interest
                                             // for this segment. if dataNonce_
                                             // and interestNonce_ coincides,
                                             // this means that the interest was
                                             // answered by a producer
                    int32_t generationDelayMs_;  // in case if segment arrived
                                                 // straight from producer, it
                                                 // puts a delay between receiving
                                                 // an interest and answering it
                                                 // into the segment's meta data
                                                 // header, otherwise - 0
                    
                    void resetData();
                }; // class Segment
                
                /**
                 * Slot namespace indicates, to which namespace does the
                 * frame/packet belong
                 */
                enum Namespace {
                    Unknown = -1,
                    Key = 0,
                    Delta = 1
                }; // enum Namespace
                
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
                }; // enum State
                
                /**
                 * Slot can have different data consistency states, i.e. as
                 * frame meta info is spread over data segments and name
                 * prefixes, some meta data is available once any segments is
                 * received (it can be retrieved from the data name prefix),
                 * other meta data is available only when segment #0 is
                 * received - as producer adds header to the frame data.
                 */
                typedef enum _Consistency {
                    Inconsistent = 0<<0,       // slot has no meta info yet
                    PrefixMeta = 1<<1,    // slot has meta data from the
                                          // name prefix
                    HeaderMeta = 1<<2,    // slot has meta data from the
                                          // header, provided by producer
                    Consistent = PrefixMeta|HeaderMeta // all meta data is ready
                } Consistency;
                
                /**
                 * Slot comparator class used for ordering slots in playback 
                 * order.
                 * Playback ordering is mainly based on playback sequence number
                 * of a frame. Sometimes, playback sequence number is not 
                 * available - in cases if the interest just has been issued but 
                 * no data has arrived yet. For such cases, one can use namespace sequence numbers if the frames are from the same namespace (i.e. either both are key or delta frames). Otherwise - comparison based on prefixes is performed - and key frame will always precede
                 */
                class PlaybackComparator {
                public:
                    PlaybackComparator(){}
                    bool operator() (const shared_ptr<Slot> &slot1,
                                     const shared_ptr<Slot> &slot2) const;
                };
                
                Slot(unsigned int segmentSize);
                ~Slot();
                
                /**
                 * Adds pending segment to the slot
                 * @param interest Interest for the data segment for the current
                 * slot
                 * @return  RESULT_OK if segment was added sucessfully,
                 *          RESULT_FAIL otherwise
                 */
                int
                addInterest(const Interest& interest);
                
                /**
                 * Marks pending segment as missing
                 * @param interest Interest used previously in addInterest call
                 * @return  RESULT_OK   if pending segment was marked as missing
                 *          RESULT_WARN there is a segment exists for current
                 *                      interest but it is has state different 
                 *                      from StatePending
                 *          RESULT_ERR  there is no segment which satisfies
                 *                      provided interest
                 */
                int
                markMissing(const Interest& interest);
                
                /**
                 * Adds data to the slot
                 * @param data Data of the segment
                 * @return  updated slot state
                 */
                State
                appendData(const Data &data);
                
                /**
                 * Marks slot as free
                 */
                int
                reset();
                
                /**
                 * Lock slot for playback
                 */
                void
                lock()
                {
                    stashedState_ = state_;
                    state_ = StateLocked;
                }
                
                /**
                 * Unlocks slot after playback
                 */
                void
                unlock() { state_ = stashedState_; }
                
                /**
                 * Returns slot's current playback deadline
                 */
                int64_t
                getPlaybackDeadline() const { return playbackDeadline_; }
                
                /**
                 * Updates slot's playback deadline
                 */
                void
                setPlaybackDeadline(int64_t playbackDeadline)
                { playbackDeadline_ = playbackDeadline; }
                
                int64_t
                getProducerTimestamp() const
                {
                    if (consistency_&HeaderMeta)
                        return producerTimestamp_;
                    return -1;
                }
                
                double
                getPacketRate() const { return packetRate_; }
                
                /**
                 * Returns current slot state
                 */
                State
                getState() const { return state_; }
                
                /**
                 * Returns current data consistency state
                 */
                int
                getConsistencyState() const { return consistency_; }
                
                /**
                 * Returns playback frame number
                 * @return Absolute frame number or -1 if necessary data has
                 * not been received yet
                 * @see getConsitencyState
                 */
                PacketNumber
                getPlaybackNumber() const {
                    if (consistency_&PrefixMeta)
                        return packetPlaybackNumber_;
                    return -1;
                }
                
                /**
                 * Returns sequential number in the current namespace
                 */
                PacketNumber
                getSequentialNumber() const {
                    return packetSequenceNumber_;
                }
                
                /**
                 * Returns current slot's namespace
                 */
                Namespace
                getNamespace() const { return packetNamespace_; }
                
                /**
                 * Returns appropriate key frame playback number
                 * For key frames: the same as getPlaybackNumber
                 * Fro delta frames: returns last key frame playback number
                 * which belongs to the same GOP as current frames
                 * @return frame number or -1 if data has not been received yet
                 * @see getConsistencyState
                 */
                PacketNumber
                getGopKeyFrameNumber() const
                {
                    if (consistency_&PrefixMeta)
                        return keySequenceNumber_;
                    return -1;
                }
                
                int
                getSegmentsNumber() const { return nSegmentsTotal_; }
                
                int
                getPacketData(PacketData** packetData) const
                {
                    if (consistency_&Consistent &&
                        nSegmentsPending_ == 0)
                        return PacketData::packetFromRaw(assembledSize_,
                                                         slotData_,
                                                         packetData);
                    return RESULT_ERR;
                }
                
                std::vector<shared_ptr<Segment>>
                getMissingSegments()
                { return getSegments(Segment::StateMissing); }
                
                std::vector<shared_ptr<Segment>>
                getPendingSegments()
                { return getSegments(Segment::StatePending); }

                std::vector<shared_ptr<Segment>>
                getFetchedSegments()
                { return getSegments(Segment::StateFetched); }
                
                double
                getAssembledLevel()
                {
                    if (!nSegmentsTotal_) return 0;
                    return (double)nSegmentsReady_/(double)nSegmentsTotal_;
                }
                
                const Name&
                getPrefix() { return slotPrefix_; }
                
                void
                setPrefix(const Name& prefix) { slotPrefix_ = prefix; }
                
                shared_ptr<Segment>
                getSegment(SegmentNumber segNo) const;
                
                shared_ptr<Segment>
                getRecentSegment() const { return recentSegment_; }
                
                bool
                isRightmost() const { return rightmostSegment_.get() != NULL; }
                
                static std::string
                stateToString(State s)
                {
                    switch (s) {
                        case StateAssembling: return "Assembling"; break;
                        case StateFree: return "Free"; break;
                        case StateLocked: return "Locked"; break;
                        case StateNew: return "New"; break;
                        case StateReady: return "Ready"; break;
                        default: return "Unknown"; break;
                    }
                }
            private:
                unsigned int segmentSize_ = 0;
                unsigned int allocatedSize_ = 0, assembledSize_ = 0;
                unsigned char* slotData_ = NULL;
                State state_, stashedState_;
                int consistency_;
                
                Name slotPrefix_;
                PacketNumber packetSequenceNumber_, keySequenceNumber_,
                                packetPlaybackNumber_;
                Namespace packetNamespace_;

                int64_t requestTimeUsec_, readyTimeUsec_;
                int64_t playbackDeadline_, producerTimestamp_;
                double packetRate_;

                int nSegmentsReady_, nSegmentsPending_, nSegmentsMissing_,
                nSegmentsTotal_;
                
                shared_ptr<Segment> rightmostSegment_, recentSegment_;
                std::vector<shared_ptr<Segment>> freeSegments_;
                std::map<SegmentNumber, shared_ptr<Segment>> activeSegments_;
                
                webrtc::CriticalSectionWrapper& accessCs_;
                
                shared_ptr<Segment>
                prepareFreeSegment(SegmentNumber segNo);
                
                shared_ptr<Segment>&
                getActiveSegment(SegmentNumber segmentNumber);
                
                void
                fixRightmost(PacketNumber packetNumber,
                             SegmentNumber segmentNumber);
                
                void
                releaseSegment(SegmentNumber segNum);
                
                void
                prepareStorage(unsigned int segmentSize,
                               unsigned int nSegments);
                
                void
                addData(const unsigned char* segmentData, unsigned int segmentSize,
                        SegmentNumber segNo, unsigned int totalSegNo);
                
                unsigned char*
                getSegmentDataPtr(unsigned int segmentSize,
                                  unsigned int segmentNumber);
                
                std::vector<shared_ptr<Segment>>
                getSegments(int segmentStateMask);
                
                void
                updateConsistencyWithMeta(const PacketNumber& sequenceNumber,
                                          const PrefixMetaInfo &prefixMeta);
                
                void
                initMissingSegments();
                
                bool
                updateConsistencyFromHeader();
            }; // class Slot
            
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
                    StateChanged   = 1<<4, // triggered when buffer state changed
                    Error          = 1<<6  // general error event
                };
                
                static const int AllEventsMask;
                static std::string toString(Event e){
                    switch (e.type_){
                        case Ready: return "Ready"; break;
                        case FirstSegment: return "FirstSegment"; break;
                        case FreeSlot: return "FreeSlot"; break;
                        case Timeout: return "Timeout"; break;
                        case StateChanged: return "StateChanged"; break;
                        case Error: return "Error"; break;
                        default: return "Unknown"; break;
                    }
                }
                
                EventType type_; // type of the event occurred
                shared_ptr<Slot> slot_;     // corresponding slot pointer
            };
            
            FrameBuffer(shared_ptr<const FetchChannel> &fetchChannel);
            ~FrameBuffer();
            
            std::string
            getDescription() const { return "framebuffer"; };
            
            int init() { reset(); initialize(); return RESULT_OK; }
            int reset();
            
            Event
            waitForEvents(int eventMask, unsigned int timeout = 0xffffffff);
            
            State
            getState() { return state_; }
            
            void
            setState(const State &state);
            
            /**
             * Releases all threads awaiting for buffer events
             */
            void
            release();
            
            std::vector<shared_ptr<Slot>>
            getActiveSlots()
            {
                return getSlots(Slot::StateNew | Slot::StateAssembling |
                                Slot::StateReady | Slot::StateLocked);
            }
            
            /**
             * Reserves slot for specified interest
             * @param interest Segment interest that has been issued. Prefix
             * will be stripped to contain only frame number (if possible) and
             * this stripped prefix will be used as a key to reserve slot.
             * @return returns state of the slot after reservation attempt:
             *          StateNew if slot has been reserved successfuly
             *          StateAssembling if slot has been already reserved
             *          StateFree if slot can't be reserved
             *          StateLocked if slot can't be reserved and is locked
             *          StateReady if slot can't be reserved and is ready
             */
            Slot::State
            interestIssued(const Interest &interest);
            
            /**
             * Appends data to the slot
             * @return returns state of the slot after operation:
             *  - StateFree slot stayed unaffected (was not reserved or not found)
             *  - StateAssembling slot is in assembling mode - waiting for more segments
             *  - StateReady slot is assembled and ready for decoding
             *  - StateLocked slot is locked and was not affected
             */
            Slot::State
            newData(const Data& data);
            
            /**
             * Frees slot for specified name prefix
             * @param prefix NDN prefix name which corresponds to the slot's
             * reservation prefix. If prefix has more components than it is
             * required (up to fram no), it will be trimmed and the rest of
             * the prefix will be used to locate reserved slot in the storage.
             */
            Slot::State
            freeSlot(const Name &prefix);
            
            /**
             * Frees every slot which is less in NDN prefix canonical ordering
             * @param prefix NDN prefix which is used for comparison. Every slot
             * which has prefix less than this prefix (in NDN canonical
             * ordering) will be freed
             * @return Number of freed slots or -1 if operation couldn't be
             * performed
             */
            int
            freeSlotsLessThan(const Name &prefix);
            
            /**
             * Sets the size of the buffer (in milliseconds).
             * Usually, buffer should collect at least this duration of frames
             * in order to start playback
             */
            void
            setTargetSize(int64_t targetSizeMs)
            { targetSizeMs_ = targetSizeMs; }

            int64_t
            getTargetSize() { return targetSizeMs_; }
            
            /**
             * Check estimated buffer size and if it's greater than targetSizeMs
             * removes frees slots starting from the end of the buffer (the end
             * contains oldest slots, beginning - newest)
             */
            void
            recycleOldSlots();
            
            /**
             * Estimates current buffer size based on current active slots
             */
            int64_t
            getEstimatedBufferSize();
            
            virtual void
            acquireSlot(PacketData** packetData);
            
            virtual int
            releaseAcquiredSlot();
            
            void
            recycleEvent(Event& event);
            
            static std::string
            stateToString(State s)
            {
                switch (s)
                {
                    case Valid: return "Valid"; break;
                    case Invalid: return "Invalid"; break;
                    default: return "Unknown"; break;
                }
            }
            
        private:
            // playback queue contains active slots sorted in ascending
            // playback order (see PlaybackComparator)
            // - each elements is a shared_ptr for the slot in activeSlots_ map
            // - std::vector is used as a container
            // - PlaybackComparator is used for ordering slots in playback order
            //            typedef std::priority_queue<shared_ptr<FrameBuffer::Slot>,
            //            std::vector<shared_ptr<FrameBuffer::Slot>>,
            //            FrameBuffer::Slot::PlaybackComparator>
            class PlaybackQueue : public std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot>>
            {
            public:
                PlaybackQueue(unsigned int playbackRate);
                
                int64_t
                getPlaybackDuration();
                
                void
                pushSlot(const shared_ptr<FrameBuffer::Slot>& slot);
                
                shared_ptr<FrameBuffer::Slot>
                popSlot();
                
                void
                updatePlaybackRate(double playbackRate);
                
                double
                getPlaybackRate() { return playbackRate_; }
                
                void
                clear();

            private:
                double playbackRate_;
                webrtc::CriticalSectionWrapper& accessCs_;
                FrameBuffer::Slot::PlaybackComparator comparator_;
                
                int64_t
                getInferredDuration() { return ceil(1000./playbackRate_); }
            };
            
            shared_ptr<const FetchChannel> fetchChannel_;
            
            State state_;
            int64_t targetSizeMs_;
            int64_t estimatedSizeMs_;
            bool isEstimationNeeded_;
            bool isWaitingForRightmost_;
            
            std::vector<shared_ptr<Slot>> freeSlots_;
            std::map<Name, shared_ptr<Slot>> activeSlots_;
            PlaybackQueue playbackQueue_;
            
            webrtc::CriticalSectionWrapper &syncCs_;
            webrtc::EventWrapper &bufferEvent_;
            
            // lock object for fetching pending buffer events
            bool forcedRelease_ = false;
            std::list<Event> pendingEvents_;
            webrtc::RWLockWrapper &bufferEventsRWLock_;
            
            shared_ptr<Slot>
            getSlot(const Name& prefix, bool remove = false,
                    bool shouldMatch = true);
            
            int
            setSlot(const Name& prefix, shared_ptr<Slot>& slot);
            
            std::vector<shared_ptr<Slot>>
            getSlots(int slotStateMask);
            
            /**
             * Estimates buffer size based on the current active slots
             */
            void
            estimateBufferSize();
            
            void
            resetData();
            
            bool
            getLookupPrefix(const Name& prefix, Name& lookupPrefix);
            
            void
            addBufferEvent(Event::EventType type, shared_ptr<Slot>& slot);
            
            void
            addStateChangedEvent(State newState);
            
            void
            initialize();
            
            Slot::State
            reserveSlot(const Interest &interest);
            
            void
            updateCurrentRate(unsigned int playbackRate)
            {
                if (playbackRate != 0 &&
                    playbackRate != playbackQueue_.getPlaybackRate())
                {
                    isEstimationNeeded_ = true;
                    playbackQueue_.updatePlaybackRate(playbackRate);
                }
            }
            
            void
            fixRightmost(const Name& prefix);
        };
    }
    
    //******************************************************************************
    //******************************************************************************
    // old api *********************************************************************
    //******************************************************************************
    //******************************************************************************

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
                StateChanged   = 1<<4, // triggered when buffer changes state
                Error          = 1<<5  // general error event
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
                    
                    return -1;
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
                        
                        bool cres = false;
                        //(metaA.sequencePacketNumber_ < metaB.sequencePacketNumber_);
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
         * Appends data segment to the slot. Triggers defferent events 
         * according to the situation. If slot is in a New state, automatically 
         * marks it as Assembling.
         * @param data Segment data fetched from NDN network
         */
        CallResult
        appendSegment(const Data &data);
        
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
