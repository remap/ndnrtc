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

#include <set>
#include <tr1/unordered_set>

#include "frame-data.h"
#include "ndnrtc-common.h"
#include "ndnrtc-utils.h"

#include "consumer.h"

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
            class Slot
            {
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
                    
                    void
                    setDataPtr(const unsigned char* dataPtr)
                    { dataPtr_ = const_cast<unsigned char*>(dataPtr); }

                    unsigned char*
                    getDataPtr() const { return dataPtr_; }

                    void
                    setNumber(SegmentNumber number) { segmentNumber_ = number; }
                    
                    SegmentNumber
                    getNumber() const { return segmentNumber_; }
                    
                    SegmentData::SegmentMetaInfo getMetadata() const;
                    
                    State
                    getState() const { return state_; };
                    
                    int64_t
                    getRequestTimeUsec()
                    { return requestTimeUsec_; }
                    
                    int64_t
                    getArrivalTimeUsec()
                    { return arrivalTimeUsec_; }
                    
                    int64_t
                    getRoundTripDelayUsec()
                    { return (arrivalTimeUsec_-requestTimeUsec_); }
                    
                    void
                    setPrefix(const Name& prefix)
                    { prefix_ = prefix; }
                    
                    const Name&
                    getPrefix() { return prefix_; };
                    
                    void
                    setIsParity(bool isParity)
                    { isParity_ = isParity; }
                    
                    bool
                    isParity()
                    { return isParity_; }
                    
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
                    bool isParity_;
                    
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
                    Inconsistent = 1<<0,       // slot has no meta info yet
                    PrefixMeta = 1<<2,    // slot has meta data from the
                                          // name prefix
                    HeaderMeta = 1<<3,    // slot has meta data from the
                                          // header, provided by producer
                    Consistent = PrefixMeta|HeaderMeta // all meta data is ready
                } Consistency;
                
                /**
                 * Slot comparator class used for ordering slots in playback 
                 * order.
                 * Playback ordering is mainly based on playback sequence number
                 * of a frame. Sometimes, playback sequence number is not 
                 * available - in cases if the interest just has been issued but 
                 * no data has arrived yet. For such cases, one can use namespace 
                 * sequence numbers if the frames are from the same namespace 
                 * (i.e. either both are key or delta frames). Otherwise - 
                 * comparison based on prefixes is performed - and key frame 
                 * will always precede
                 */
                class PlaybackComparator {
                public:
                    PlaybackComparator(bool inverted = false):inverted_(inverted){}
                    bool operator() (const Slot* slot1,
                                     const Slot* slot2) const;
                    
                private:
                    bool inverted_;
                };
                
                Slot(unsigned int segmentSize, bool useFec);
                ~Slot();
                
                /**
                 * Adds pending segment to the slot
                 * @param interest Interest for the data segment for the current
                 * slot. This call will set interest's nonce value
                 * @return  RESULT_OK if segment was added sucessfully,
                 *          RESULT_FAIL otherwise
                 */
                int
                addInterest(Interest& interest);
                
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
                 * Applies FEC recovering to the frame if it's not fully 
                 * assembled
                 * @return number of recovered segments
                 */
                int
                recover();
                
                /**
                 * Returns slot's current playback deadline. Playback deadline 
                 * defines how far (in milliseconds) is the frame from the 
                 * current playhead. In other words - playback deadline defines 
                 * delay after which current frame should be played out
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
                 * Returns appropriate frame sequence number from the paired 
                 * namespace (delta for key frames and vice versa)
                 * For key frames: sequence number of first delta frame in the 
                 * gop
                 * Fro delta frames: returns key frame sequence number from the 
                 * gop of current delta frame
                 * @return frame number or -1 if data has not been received yet
                 * @see getConsistencyState
                 */
                PacketNumber
                getPairedFrameNumber() const
                {
                    if (consistency_&PrefixMeta)
                        return pairedSequenceNumber_;
                    return -1;
                }
                
                int
                getSegmentsNumber() const { return nSegmentsTotal_; }
                
                int
                getParitySegmentsNumber() const { return nSegmentsParity_; }
                
                int
                getPacketData(PacketData** packetData) const
                {
#warning put back checking
                    if (consistency_&Consistent)// &&
//                        nSegmentsPending_ == 0)
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
                
                bool
                isRecovered()
                { return isRecovered_; }
                
                const Name&
                getPrefix() const { return slotPrefix_; }
                
                void
                setPrefix(const Name& prefix);
                
                shared_ptr<Segment>
                getSegment(SegmentNumber segNo, bool isParity) const;
                
                shared_ptr<Segment>
                getRecentSegment() const { return recentSegment_; }
                
                bool
                isRightmost() const { return rightmostSegment_.get() != NULL; }
                
                unsigned int
                getRtxNum(){ return nRtx_; }
                
                void
                incremenrRtxNum() { nRtx_++; }
                
                int64_t
                getLifetime()
                { return (requestTimeUsec_ > 0)?NdnRtcUtils::millisecondTimestamp()-requestTimeUsec_/1000.:0;}
                
                int64_t
                getAssemblingTime()
                { return ((readyTimeUsec_ >= 0) ? (readyTimeUsec_-firstSegmentTimeUsec_)/1000. : 0); }
                
                bool
                hasOriginalSegments() { return hasOriginalSegments_; }
                
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
                
                static std::string
                toString(int consistency)
                {
                    std::stringstream str;
                    if (consistency == Consistent)
                        str << "C";
                    else if (consistency == Inconsistent)
                        str << "I";
                    else
                    {
                        if (consistency&PrefixMeta)
                            str << "P";
                        if (consistency&HeaderMeta)
                            str << "H";
                    }
                    return str.str();
                }
                
                static bool
                isNextPacket(const Slot& slot, const Slot& nextSlot);
                
                std::string
                dump();
                
//                unsigned char*
//                getDataPtr() { return slotData_; }
                
            private:
                unsigned int segmentSize_ = 0;
                unsigned int allocatedSize_ = 0, assembledSize_ = 0;
                unsigned char* slotData_ = NULL;
                unsigned char* fecSegmentList_ = NULL;
                
                State state_, stashedState_;
                int consistency_;
                unsigned int nRtx_;
                
                bool hasOriginalSegments_, isRecovered_, useFec_;
                
                Name slotPrefix_;
                PacketNumber packetSequenceNumber_, pairedSequenceNumber_,
                                packetPlaybackNumber_;
                Namespace packetNamespace_;

                int64_t requestTimeUsec_, firstSegmentTimeUsec_, readyTimeUsec_;
                int64_t playbackDeadline_, producerTimestamp_;
                double packetRate_;

                int nSegmentsReady_ = 0, nSegmentsPending_ = 0,
                nSegmentsMissing_ = 0, nSegmentsTotal_ = 0,
                nSegmentsParity_ = 0, nSegmentsParityReady_ = 0;
                
                shared_ptr<Segment> rightmostSegment_, recentSegment_;
                std::vector<shared_ptr<Segment>> freeSegments_;
                std::map<SegmentNumber, shared_ptr<Segment>> activeSegments_;
                
                shared_ptr<Segment>
                prepareFreeSegment(SegmentNumber segNo, bool isParity);
                
                shared_ptr<Segment>&
                getActiveSegment(SegmentNumber segmentNumber, bool isParity);
                
                void
                fixRightmost(PacketNumber packetNumber,
                             SegmentNumber segmentNumber, bool isParity);
                
                void
                releaseSegment(SegmentNumber segNum);
                
                void
                prepareStorage(unsigned int segmentSize,
                               unsigned int nSegments,
                               unsigned int nParitySegments);
                
                void
                addData(const unsigned char* segmentData, unsigned int segmentSize,
                        SegmentNumber segNo, unsigned int totalSegNo,
                        bool isParitySegment = false,
                        unsigned int totalParitySegNum = 0);
                
                unsigned char*
                getSegmentDataPtr(unsigned int segmentSize,
                                  SegmentNumber segmentNumber,
                                  bool isParity);
                
                std::vector<shared_ptr<Segment>>
                getSegments(int segmentStateMask);
                
                void
                updateConsistencyWithMeta(const PacketNumber& sequenceNumber,
                                          const PrefixMetaInfo &prefixMeta);
                
                void
                refineActiveSegments();
                
                void
                initMissingSegments();

                void
                initMissingParitySegments();
                
                bool
                updateConsistencyFromHeader();
                
                SegmentNumber
                toMapParityIdx(SegmentNumber segNo) const
                { return -(segNo+1); }
                
                SegmentNumber
                fromMapParityIdx(SegmentNumber segNo)
                { return -(segNo+1); }
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
                    FreeSlot       = 1<<2, // new slot free for frame assembling
                                           // ! slot attribute is unreliable
                    Timeout        = 1<<3, // triggered when timeout
                                           // happens for any segment
                                           // interest
                    StateChanged   = 1<<4, // triggered when buffer state changed
                                           // ! slot attribute is unreliable
                    Playout        = 1<<5, // frame was taken for playout (locked)
                    Underrun       = 1<<6, // frame buffer underrun (zero size)
                                           // ! slot attribute is unreliable                    
                    NeedKey        = 1<<7, // frame buffer asks for key frame.
                                           // ! slot attribute is unreliable
                    Error          = 1<<8  // general error event
                };
                
                static const int AllEventsMask;
                static std::string toString(Event e){
                    switch (e.type_){
                        case Ready: return "Ready"; break;
                        case FirstSegment: return "FirstSegment"; break;
                        case FreeSlot: return "FreeSlot"; break;
                        case Timeout: return "Timeout"; break;
                        case StateChanged: return "StateChanged"; break;
                        case Playout: return "Playout"; break;
                        case Underrun: return "Underrun"; break;
                        case NeedKey: return "NeedKey"; break;
                        case Error: return "Error"; break;
                        default: return "Unknown"; break;
                    }
                }
                
                EventType type_; // type of the event occurred
                shared_ptr<Slot> slot_;     // corresponding slot pointer
            }; // Event
            
            FrameBuffer(const shared_ptr<const Consumer> &consumer);
            ~FrameBuffer();
            
            int init();
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
            
            unsigned int
            getActiveSlotsNum()
            {
                webrtc::CriticalSectionScoped scopedCs(&syncCs_);
                return getSlots(Slot::StateNew | Slot::StateAssembling |
                                Slot::StateReady | Slot::StateLocked).size();
            }
            
            /**
             * Reserves slot for specified interest
             * @param interest Segment interest that has been issued. Prefix
             * will be stripped to contain only frame number (if possible) and
             * this stripped prefix will be used as a key to reserve slot. This 
             * call will set interest's nonce value.
             * @return returns state of the slot after reservation attempt:
             *          StateNew if slot has been reserved successfuly
             *          StateAssembling if slot has been already reserved
             *          StateFree if slot can't be reserved
             *          StateLocked if slot can't be reserved and is locked
             *          StateReady if slot can't be reserved and is ready
             */
            Slot::State
            interestIssued(Interest &interest);
            
            Slot::State
            interestRangeIssued(const Interest &packetInterest,
                                SegmentNumber startSegmentNo,
                                SegmentNumber endSegmentNo,
                                std::vector<shared_ptr<Interest>> &segmentInterests,
                                bool isParity);
            
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
            
            void
            interestTimeout(const Interest& interest);
            
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
             * Checks estimated buffer size and if it's greater than targetSizeMs
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

            /**
             * Returns actually playbale frames duration from buffer
             */
            int64_t
            getPlayableBufferSize();
            
            /**
             * Acquires current top slot from the playback queue for playback.
             * Acquired slot will be locked unless corresponding 
             * releaseAcquiredSlot method will be called. One can rely on slot's
             * data consistency between these calls. That's why passing a frame
             * into decoder should be implemented between these calls.
             * @param packetData Pointer to the packet's data, where frame's 
             *                   data wil be extracted
             * @param assembledLevel Frame assembled level, i.e. ratio (0<r<1) 
             *                       of how many segments were assembled. 1 
             *                       means frame was fully assembled, 0 means 
             *                       frame was not assembled at all (most likely
             *                       in this case, *packetData will be null).
             */
            virtual void
            acquireSlot(ndnrtc::PacketData **packetData,
                        PacketNumber& packetNo,
                        PacketNumber& sequencePacketNo,
                        PacketNumber& pairedPacketNo,
                        bool& isKey, double& assembledLevel);
            
            /**
             * Releases previously acquired slot.
             * After this call, slot will be unlocked, reset and freed in the
             * buffer so it can be re-used for new frames. One should not rely 
             * on data slot's consistency after this call.
             * @param (out) isInferredPlayback Indicates, whether the value 
             * returned is an inferred playback duration (as opposed to duration 
             * calculated using timestamp difference).
             * @return Playback duration
             */
            virtual int
            releaseAcquiredSlot(bool& isInferredPlayback);
            
            void
            recycleEvent(const Event& event);
            
            void
            synchronizeAcquire() { syncCs_.Enter(); }
            
            void
            synchronizeRelease() { syncCs_.Leave(); }
    
            void
            dump();
            
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
            
            double
            getCurrentRate() const
            { return playbackQueue_.getPlaybackRate(); }
            
            void
            setDescription(const std::string& desc);
            
            void
            setLogger(ndnlog::new_api::Logger* logger);
            
            void
            getStatistics(ReceiverChannelPerformance& stat);
            
        protected:
            // playback queue contains active slots sorted in ascending
            // playback order (see PlaybackComparator)
            // - each elements is a shared_ptr for the slot in activeSlots_ map
            // - std::vector is used as a container
            // - PlaybackComparator is used for ordering slots in playback order
            //            typedef std::priority_queue<shared_ptr<FrameBuffer::Slot>,
            //            std::vector<shared_ptr<FrameBuffer::Slot>>,
            //            FrameBuffer::Slot::PlaybackComparator>
            typedef
            std::vector<ndnrtc::new_api::FrameBuffer::Slot*>
            PlaybackQueueBase;
            
            class PlaybackQueue : public PlaybackQueueBase,
            public ndnlog::new_api::ILoggingObject
            {
            public:
                PlaybackQueue(double playbackRate);
                
                int64_t
                getPlaybackDuration(bool estimate = true);
                
                void
                updatePlaybackDeadlines();
                
                void
                pushSlot(FrameBuffer::Slot* const slot);

                FrameBuffer::Slot*
                peekSlot();
                
                void
                popSlot();
                
                void
                updatePlaybackRate(double playbackRate);
                
                double
                getPlaybackRate() const { return playbackRate_; }
                
                void
                clear();
                
                int64_t
                getInferredFrameDuration()
                { /*return lastFrameDuration_;*/
                    return ceil(1000./playbackRate_);
                }

                void
                dumpQueue();
                
                std::string
                dumpShort();
                
            private:
                double playbackRate_;
                int64_t lastFrameDuration_;
                FrameBuffer::Slot::PlaybackComparator comparator_;
                
                void
                sort();
            };
            
            const Consumer* consumer_;
            
            State state_;
            PacketNumber playbackNo_;
            int nKeyFrames_;
            unsigned int nReceivedFrames_, nRescuedFrames_,
                nIncomplete_, nRecovered_;
            
            // flag which determines whether currently acquired packet should
            // be skipped (in case of old slot acquisition)
            bool skipFrame_ = false;
            shared_ptr<Slot> playbackSlot_;
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
            
            /**
             * Frees slot for specified name prefix
             * @param prefix NDN prefix name which corresponds to the slot's
             * reservation prefix. If prefix has more components than it is
             * required (up to fram no), it will be trimmed and the rest of
             * the prefix will be used to locate reserved slot in the storage.
             */
            Slot::State
            freeSlot(const Name &prefix);
            
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
            addBufferEvent(Event::EventType type, const shared_ptr<Slot>& slot);
            
            void
            addStateChangedEvent(State newState);
            
            void
            initialize();
            
            shared_ptr<Slot>
            reserveSlot(const Interest &interest);
            
            void
            updateCurrentRate(double playbackRate)
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
            
            void
            dumpActiveSlots();
        };
    }
}

#endif /* defined(__ndnrtc__frame_buffer__) */
