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

#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <ndn-cpp/name.hpp>

#include "name-components.h"

#include "slot-buffer.h"
#include "ndnrtc-object.h"

namespace ndn {
    class Interest;
    class Data;
    class Name;
}

namespace ndnrtc
{
    struct _CommonHeader;
    class WireSegment;

    class SlotSegment {
    public:

        SlotSegment(const boost::shared_ptr<ndn::Interest>&);

        const NamespaceInfo& getInfo() const;
        void setData(const boost::shared_ptr<WireSegment>& data);
        const boost::shared_ptr<WireSegment>& getData() const { return data_; }
        
        bool isFetched() const { return data_.get(); }
        bool isPending() const { return !data_.get(); }
        bool isRightmostRequested() const { return !interestInfo_.hasSeqNo_; }
        bool isOriginal() const;

        int64_t getRequestTimeUsec() const { return requestTimeUsec_; }
        
        int64_t getArrivalTimeUsec() const { return arrivalTimeUsec_; }
        
        int64_t getRoundTripDelayUsec() const
        {
            if (arrivalTimeUsec_ <= 0) return -1;
            return (arrivalTimeUsec_-requestTimeUsec_);
        }

    private:
        boost::shared_ptr<ndn::Interest> interest_;
        NamespaceInfo interestInfo_;
        boost::shared_ptr<WireSegment> data_;
        int64_t requestTimeUsec_, arrivalTimeUsec_;
        size_t requestNo_;
        bool isVerified_;
    };

    //******************************************************************************
    class VideoFrameSlot;
    class AudioBundleSlot;
    class Buffer;

    class BufferSlot : public ISlot
    {
    public:
        enum State {
            Free = 1<<0,  // slot is free for being used
            New = 1<<1,   // slot is being used for assembling, but has
                            // not recevied any data segments yet
            Assembling = 1<<2,    // slot is being used for assembling and
                                    // already has some data segments arrived
            Ready = 1<<3, // slot assembled all the data and is ready for
                            // decoding a frame
            Locked = 1<<4 // slot is locked for decoding
        }; // enum State
        
        typedef enum _Consistency {
            Inconsistent = 0,        // slot has no meta info yet
            SegmentMeta = 1<<1,         // slot has meta extracted from 
                                        // segment header
            HeaderMeta = 1<<2,    // slot has meta data from the
                                  // header, provided by producer
            Consistent = SegmentMeta|HeaderMeta // all meta data is ready
        } Consistency;

        BufferSlot();
        ~BufferSlot(){}
        
        /**
         * Adds issued Intersts to this slot.
         * @param interests Vector of Interests' shared pointers
         * @note Interst names should relate to the same sample, and have segment
         * numbers (no rightmost Interests allowed). Otherwise, this throws an 
         * exception (basic guarantee - one need to clear slot in order to re-use it
         * again).
         * @see clear()
         */
        void 
        segmentsRequested(const std::vector<boost::shared_ptr<ndn::Interest>>& interests);
        
        /**
         * Clears all internal structures of this slot and returns to Free state
         * as if slot has just been created. No memory deallocation/reallocation is
         * performed, thus operation is not expensive.
         */
        void
        clear();

        /**
         * Adds received segment to this slot.
         * @param segment Received segment
         * @note Added segment's name should correspond to one of the Interests, 
         * previously added using segmentsRequested method. Otherwise, this throws.
         * Priovides strong guarantee.
         * @return Slot segment that contains received segment and previously 
         * expressed Interest; it can be used for retrieving segment-level metadata
         * like Data Retrieval Delays, etc.
         */
        const boost::shared_ptr<SlotSegment>
        segmentReceived(const boost::shared_ptr<WireSegment>& segment);

        void
        markVerified(const std::string& segmentName, bool verified) {}

        State getState() const { return state_; }
        
        double getAssembledLevel();
        double getAssembledLevel() const { return asmLevel_; }

        const ndn::Name& getPrefix() const { return name_; }
        const NamespaceInfo& getNameInfo() const { return nameInfo_; }
        int getConsistencyState() const { return consistency_; }
        unsigned int getRtxNum() const { return nRtx_; }
        int64_t getAssemblingTime() const { return ( state_ >= Ready ? assembledTimeUsec_-firstSegmentTimeUsec_ : 0); }
        int64_t getShortestDrd() const { return (state_ >= Assembling ? firstSegmentTimeUsec_-requestTimeUsec_ : 0); }
        int64_t getLongestDrd() const { return (state_ >= Ready ? assembledTimeUsec_ - requestTimeUsec_ : 0); }
        bool hasOriginalSegments() const { return hasOriginalSegments_; }
        /**
         * Returns common packet header if it's available (HeaderMeta consistency),
         * otherwise throws an error.
         * @return CommonHeader structure
         * @see CommonSamplePacket
         */
        const _CommonHeader getHeader() const;

        std::string
        dump(bool showLastSegment = false) const;

    private:
        friend VideoFrameSlot;
        friend AudioBundleSlot;
        friend Buffer;

        ndn::Name name_;
        NamespaceInfo nameInfo_;
        std::map<ndn::Name, boost::shared_ptr<SlotSegment>> requested_, fetched_;
        boost::shared_ptr<SlotSegment> lastFetched_;
        unsigned int consistency_, nRtx_, assembledSize_, nDataSegments_;
        bool hasOriginalSegments_;
        State state_;
        int64_t requestTimeUsec_, firstSegmentTimeUsec_, assembledTimeUsec_;
        double assembled_, asmLevel_;

        virtual void updateConsistencyState(const boost::shared_ptr<SlotSegment>& segment);
    };

    //******************************************************************************
    template<typename T>
    class VideoFramePacketT;
    struct Immutable;
    typedef VideoFramePacketT<Immutable> ImmutableFrameAlias;
    struct _VideoFrameSegmentHeader;

    class VideoFrameSlot {
    public:
        VideoFrameSlot(const size_t storageSize = 16000);

        /**
         * Tries to read VideoFramePacket from supplied BufferSlot.
         * Also tries to recover frame using available FEC data, if possible.
         * In this case, recovered flag is set to true;
         * @param slot Buffer slot that contains segments of video frame packet
         * @return shared_ptr of ImmutableVideoFramePacket or nullptr if 
         * recovery attempt failed
         */
        boost::shared_ptr<ImmutableFrameAlias>
        readPacket(const BufferSlot& slot, bool& recovered);

        _VideoFrameSegmentHeader 
        readSegmentHeader(const BufferSlot& slot);
        
    private:
        boost::shared_ptr<std::vector<uint8_t>> storage_;
        std::vector<uint8_t> fecList_;
    };

    //******************************************************************************
    template<typename T>
    class AudioBundlePacketT;
    typedef AudioBundlePacketT<Immutable> ImmutableAudioBundleAlias;

    class AudioBundleSlot {
    public:
        AudioBundleSlot(const size_t storageSize = 2000);

        /**
         * Tries to read AudioBundlePacket from supplied BufferSlot.
         * @param slot Buffer slot that contains segment(s) of audio bundle
         * @return shared_ptr of ImmutableAudioBundle packet or nullptr if 
         * failed to read data.
         */
        boost::shared_ptr<ImmutableAudioBundleAlias>
        readBundle(const BufferSlot& slot);

    private:
        boost::shared_ptr<std::vector<uint8_t>> storage_;
    };

    //******************************************************************************
    class SlotPool {
    public:
        SlotPool(const size_t& capacity = 300);
            
        boost::shared_ptr<BufferSlot> pop();
        bool push(const boost::shared_ptr<BufferSlot>& slot);

        size_t capacity() const { return capacity_; }
        size_t size() const { return pool_.size(); }

    private:
        SlotPool(const SlotPool&) = delete;

        size_t capacity_;
        std::vector<boost::shared_ptr<BufferSlot>> pool_;
    };

    //******************************************************************************
    class IBufferObserver;
    class PlaybackQueue;

    class Buffer : public NdnRtcComponent {
    public:
        typedef struct _Receipt {
            boost::shared_ptr<const BufferSlot> slot_;
            boost::shared_ptr<const SlotSegment> segment_;
        } Receipt;

        Buffer(boost::shared_ptr<SlotPool> pool = 
                boost::shared_ptr<SlotPool>(new SlotPool()));

        void reset();

        bool
        requested(const std::vector<boost::shared_ptr<ndn::Interest>>& interests);

        Receipt
        received(const boost::shared_ptr<WireSegment>& segment);

        unsigned int getSlotsNum(const ndn::Name& prefix, int stateMask);
        void attach(IBufferObserver* observer);
        void detach(IBufferObserver* observer);
        boost::shared_ptr<SlotPool> getPool() const { return pool_; }

        std::string
        dump() const;

        std::string
        shortdump() const;

    private:
        friend PlaybackQueue;

        mutable boost::recursive_mutex mutex_;
        boost::shared_ptr<SlotPool> pool_;
        std::map<ndn::Name, boost::shared_ptr<BufferSlot>> activeSlots_;
        std::vector<IBufferObserver*> observers_;

        void lock(const ndn::Name& slotPrefix);
        void invalidate(const ndn::Name& slotPrefix);
        void invalidatePrevious(const ndn::Name& slotPrefix);
    };

    class IBufferObserver {
    public:
        virtual void onNewRequest(const boost::shared_ptr<BufferSlot>&) = 0;
        virtual void onNewData(const Buffer::Receipt& receipt) = 0;
    };

    //******************************************************************************
    typedef boost::function<void(const boost::shared_ptr<const BufferSlot>& slot, double playTimeMs)> ExtractSlot;
    class IPlaybackQueueObserver;

    class PlaybackQueue : public NdnRtcComponent,
                          public IBufferObserver
    {
    public:
        PlaybackQueue(const ndn::Name& streamPrefix,
            const boost::shared_ptr<Buffer>& buffer);
        ~PlaybackQueue();

        void
        pop(ExtractSlot extract);

        /**
         * This returns size in milliseconds of actual playable content
         * @return Duration in milliseconds of playable content
         */
        int64_t size() const;

        /**
         * This returns size in milliseconds of (estimated) pending content - 
         * the content that has not arrived from network yet.
         */
        int64_t pendingSize() const;

        void attach(IPlaybackQueueObserver* observer);
        void detach(IPlaybackQueueObserver* observer);

        double sampleRate() const { return packetRate_; }
        double samplePeriod() const { return (packetRate_ ? 1000./packetRate_ : 0); }

        std::string dump();

    private:
        class Sample {
        public:
            Sample(const boost::shared_ptr<const BufferSlot>& slot):slot_(slot){}

            boost::shared_ptr<const BufferSlot> slot() const { return slot_; }
            int64_t timestamp() const;
            bool operator<(const Sample& sample) const
            { return this->timestamp() < sample.timestamp(); }
        
        private:
            boost::shared_ptr<const BufferSlot> slot_;
        };

        mutable boost::recursive_mutex mutex_;
        ndn::Name streamPrefix_;
        boost::shared_ptr<Buffer> buffer_;
        double packetRate_;
        std::set<Sample> queue_;
        std::vector<IPlaybackQueueObserver*> observers_;

        virtual void onNewRequest(const boost::shared_ptr<BufferSlot>&);
        virtual void onNewData(const Buffer::Receipt& receipt);
    };

    class IPlaybackQueueObserver
    {
    public:
        virtual void onNewSampleReady() = 0;
    };
}

#endif /* defined(__ndnrtc__frame_buffer__) */
