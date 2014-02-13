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

#include "ndnrtc-common.h"
#include "ndnrtc-utils.h"

#define NDNRTC_FRAMEHDR_MRKR 0xf4d4
#define NDNRTC_FRAMEBODY_MRKR 0xfb0d
#define NDNRTC_AUDIOHDR_MRKR 0xa4a4
#define NDNRTC_AUDIOBODY_MRKR 0xabad

#define FULL_TIMEOUT_CASE_THRESHOLD 2

namespace ndnrtc
{
    typedef int PacketNumber;
    typedef int SegmentNumber;
    typedef PacketNumber FrameNumber;
    
    /**
     * Base class for storing media data for publishing in ndn
     */
    class PacketData
    {
    public:
        struct PacketMetadata {
            double packetRate_;         // current packet production rate
            PacketNumber sequencePacketNumber_;  // current packet sequence number
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
     * Class is used for packaging encoded frame metadata and actual data for 
     * transferring over the network.
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
    
    /**
     * Class is used for packaging audio samples actual data and metadata for 
     * transferring over the network.
     */
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
    
    
    /**
     * Frame assembling buffer used for assembling frames and preparing them 
     * for playback. During frame assembling, slot will transit through several
     * states: Free -> New -> Assembling -> Ready -> Locked -> Free. After 
     * decoding, slot should be unlocked and marked as free again for reuse.
     * Before issuing an interest for the frame segment, buffer slot should be 
     * booked for the specified frame number (slot will switch from Free state 
     * to New). Upon receiving first segment of the frame, slot should be 
     * marked assembling (switch to Assembling state). If the frame consists 
     * only from 1 segment, frame will switch directly to Ready state, 
     * otherwise it will be ready for decodeing upon receiving all the missing 
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
            
            bool isKeyFrame_ = false;   // indicates, whether event has occurred
                                        // due to changes in the frame of the
                                        // key namespace
            
            EventType type_;            // type of the event occurred
            SegmentNumber segmentNo_;   // segment number which triggered the
                                        // event
            PacketNumber frameNo_;      // frame number
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
            
            Slot::State getState() const { return state_; }
            PacketNumber getFrameNumber() const { return frameNumber_; }
            unsigned int assembledSegmentsNumber() const { return storedSegments_; }
            unsigned int totalSegmentsNumber() const { return segmentsNum_; }
            
            void markFree() { state_ = StateFree; }
            void markNew(PacketNumber frameNumber) {
                frameNumber_ = frameNumber;
                state_ = StateNew;
                assembledDataSize_ = 0;
                storedSegments_ = 0;
                segmentsNum_ = 0;
                segmentSize_ = 0;
                nRetransmitRequested_ = 0;
            }
            void markLocked() {
                stashedState_ = state_;
                state_ = StateLocked;
            }
            void markUnlocked() { state_ = stashedState_; }
            void markAssembling(unsigned int segmentsNum,
                                unsigned int segmentSize);
            void rename(PacketNumber newFrameNumber) {
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
            int getPacketMetadata(PacketData::PacketMetadata &metadata) const;
            
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
            State appendSegment(SegmentNumber segmentNo, unsigned int dataLength,
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
            std::tr1::unordered_set<SegmentNumber> getLateSegments(){
                webrtc::CriticalSectionScoped scopedCs(&cs_);
                return std::tr1::unordered_set<SegmentNumber>(missingSegments_);
            }
            int getRetransmitRequestedNum() {
                return nRetransmitRequested_;
            }
            void incRetransmitRequestedNum()
            {
                nRetransmitRequested_++;
            }
            
        private:
            bool isKeyFrame_;
            PacketNumber frameNumber_;
            unsigned int dataLength_;
            unsigned int assembledDataSize_, storedSegments_, segmentsNum_;
            unsigned int segmentSize_;
            uint64_t startAssemblingTs_;
            uint64_t assemblingTime_;
            unsigned char *data_;
            Slot::State state_, stashedState_;
            webrtc::CriticalSectionWrapper &cs_;
            int nRetransmitRequested_ = 0;
            std::tr1::unordered_set<SegmentNumber> missingSegments_;
            
            void flushData() { memset(data_, 0, dataLength_); }
        };
        
        FrameBuffer();
        ~FrameBuffer();
        
        int init(unsigned int bufferSize, unsigned int slotSize);
        /**
         * Flushes buffer except currently locked frames
         */
        void flush();
        /**
         * Releases currently locked waiting threads
         */
        void release();
        
        /**
         * *Blocking call* 
         * Calling thread is blocked on this call unless any type of the
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
         * @param useKeyNamespace Indicates, whether key namespace should be 
         * used or not
         * @return  CallResultNew if can slot was booked succesfully
         *          CallResultFull if buffer is full and there are no free slots available
         *          CallResultBooked if slot is already booked
         */
        CallResult bookSlot(PacketNumber frameNumber,
                            bool useKeyNamespace = false);
        
        /**
         * Marks slot for specified frame as free
         * @param frameNumber Number of frame which slot should be marked as free
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void markSlotFree(PacketNumber frameNumber,
                          bool useKeyNamespace = false);
        
        /**
         * Locks slot while it is being used by caller (for decoding for example).
         * Lock slots are not writeable and cannot be marked as free unless explicitly unlocked.
         * @param frameNumber Number of frame which slot should be locked
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void lockSlot(PacketNumber frameNumber,
                      bool useKeyNamespace = false);
        /**
         * Unocks previously locked slot. Does nothing if slot was not locked previously.
         * @param frameNumber Number of frame which slot should be unlocked
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void unlockSlot(PacketNumber frameNumber,
                        bool useKeyNamespace = false);
        
        /**
         * Marks slot as being assembled - when first segment arrived (in order to initialize slot
         * buffer properly)
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void markSlotAssembling(PacketNumber frameNumber,
                                unsigned int totalSegments,
                                unsigned int segmentSize,
                                bool useKeyNamespace = false);
        
        /**
         * Appends segment to the slot. Triggers defferent events according to situation
         * @param frameNumber Number of the frame
         * @param segmentNumber Number of the frame's segment
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        CallResult appendSegment(PacketNumber frameNumber,
                                 SegmentNumber segmentNumber,
                                 unsigned int dataLength,
                                 const unsigned char *data,
                                 bool useKeyNamespace = false);
        
        /**
         * Notifies awaiting thread that the arrival of a segment of the booked slot was timeouted
         * @param frameNumber Number of the frame
         * @param segmentNumber Number of the frame's segment
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void notifySegmentTimeout(PacketNumber frameNumber,
                                  SegmentNumber segmentNumber,
                                  bool useKeyNamespace = false);
        
        /**
         * Returns state of slot for the specified frame number
         * @param frameNo Frame number
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        Slot::State getState(PacketNumber frameNo, bool useKeyNamespace);
        
        /**
         * Returns encoded frame if it is already assembled. Otherwise - null
         * @param frameNo Frame number
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        shared_ptr<webrtc::EncodedImage> getEncodedImage(PacketNumber frameNo,
                                                         bool useKeyNamespace);
        
        /**
         * Sets new frame number for booked slot
         * @param oldFrameNo Frame number to be renamed
         * @param newFrameNo Frame number to rename by
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        void renameSlot(PacketNumber oldFrameNo, PacketNumber newFrameNo,
                        bool useKeyNamespace = false);
        
        /**
         * Buffer size
         */
        unsigned int getBufferSize() { return bufferSize_; }
        
        /**
         * Returns number of slots in specified state
         */
        unsigned int getStat(Slot::State state);
        
        /**
         * As buffer provides events-per-caller, once they emmited, they can not 
         * be re-issued anymore. In case when calledr does not want to dispatch 
         * buffer event, it should call this method so event can be re-emmited 
         * by buffer.
         */
        void reuseEvent(Event &event);
        
        /**
         * Retireves current slot from the buffer according to the frame 
         * number provided
         * @param frameNo Frame number corresponding to the slot
         * @param useKeyNamespace Indicates, whether key namespace should be
         * used or not
         */
        shared_ptr<Slot> getSlot(PacketNumber frameNo, bool useKeyNamespace)
        {
            shared_ptr<Slot> slot(nullptr);
            getFrameSlot(frameNo, &slot);
            return slot;
        }
        
    private:
        bool forcedRelease_;
        unsigned int bufferSize_, slotSize_;
        
        std::vector<shared_ptr<Slot>> freeSlots_;
        std::map<PacketNumber, shared_ptr<Slot>> frameSlotMapping_;

        // statistics
        std::map<Slot::State, unsigned int> statistics_; // stores number of frames in each state

        
        webrtc::CriticalSectionWrapper &syncCs_;
        webrtc::EventWrapper &bufferEvent_;
        
        // lock object for fetching pending buffer events
        std::list<Event> pendingEvents_;
        webrtc::RWLockWrapper &bufferEventsRWLock_;
        
        void notifyBufferEventOccurred(PacketNumber frameNo,
                                       SegmentNumber segmentNo,
                                       FrameBuffer::Event::EventType eType,
                                       Slot *slot);
        CallResult getFrameSlot(PacketNumber frameNo, shared_ptr<Slot> *slot,
                                bool remove = false);
        
        void updateStat(Slot::State state, int change);
    };
}

#endif /* defined(__ndnrtc__frame_buffer__) */
