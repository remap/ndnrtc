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

#include "name-components.hpp"

#include "slot-buffer.hpp"
#include "ndnrtc-object.hpp"

namespace ndn {
    class Interest;
    class Data;
    class Name;
}

namespace ndnrtc
{
    namespace statistics {
        class StatisticsStorage;
    }
    
    struct _CommonHeader;
    class WireSegment;

    class SlotSegment {
    public:

        SlotSegment(const boost::shared_ptr<const ndn::Interest>&);

        const NamespaceInfo& getInfo() const;
        void setData(const boost::shared_ptr<WireSegment>& data);
        const boost::shared_ptr<WireSegment>& getData() const { return data_; }
        
        bool isFetched() const { return data_.get(); }
        bool isPending() const { return !data_.get(); }
        bool isRightmostRequested() const { return !interestInfo_.hasSeqNo_; }
        bool isOriginal() const;

        int64_t getRequestTimeUsec() const { return requestTimeUsec_; }
        
        int64_t getArrivalTimeUsec() const { return arrivalTimeUsec_; }
        
        void incrementRequestNum() { requestNo_++; }
        size_t getRequestNum() const { return requestNo_; }

        /**
         * Returns round-trip time delay in microseconds if data has arrived.
         * Otherwise, returns -1.
         */
        int64_t getRoundTripDelayUsec() const
        {
            if (arrivalTimeUsec_ <= 0) return -1;
            return (arrivalTimeUsec_-requestTimeUsec_);
        }
        
        /**
         * Returns interest used to fetch this segment
         */
        boost::shared_ptr<const ndn::Interest> getInterest() const { return interest_; }

        /**
         * Takes into account if the segment is original or not.
         * If the segment is original, this returns getRoundTripDelayUsec() minus
         * generation delay received in metadata for the segment.
         */
        int64_t getDrdUsec() const;
        int64_t getDgen() const;

    private:
        boost::shared_ptr<const ndn::Interest> interest_;
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
    class Manifest;
    class SampleValidator;
    class ManifestValidator;

    class BufferSlot
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
            Locked = 1<<4 // slot is locked for playout
        }; // enum State

        enum Verification {
            Unknown = 1<<0,
            Failed = 1<<1,
            Verified = 1<<2
        };
        
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
         * Adds issued Interests to this slot.
         * @param interests Vector of Interests' shared pointers
         * @note Interst names should relate to the same sample, and have segment
         * numbers (no rightmost Interests allowed). Otherwise, this throws an 
         * exception (basic guarantee - one need to clear slot in order to re-use it
         * again).
         * @see clear()
         */
        void 
        segmentsRequested(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests);
        
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
         * Provides strong guarantee.
         * @return Slot segment that contains received segment and previously 
         * expressed Interest; it can be used for retrieving segment-level metadata
         * like Data Retrieval Delays, etc.
         */
        const boost::shared_ptr<SlotSegment>
        segmentReceived(const boost::shared_ptr<WireSegment>& segment);
        
        /**
         * Returns an array of names of missing segments
         */
        std::vector<ndn::Name> getMissingSegments() const;

        /**
         * Returns an array of pending Interests for this slot
         */
        const std::vector<boost::shared_ptr<const ndn::Interest>> getPendingInterests() const;

        /**
         * 
         */
        const std::vector<boost::shared_ptr<const SlotSegment>> getFetchedSegments() const;

        /**
         * Returns boolean value on whether slot is verified
         */
        Verification getVerificationStatus() const { return verified_; }

        State getState() const { return state_; }
        
        double getAssembledLevel() const { return asmLevel_; }

        const ndn::Name& getPrefix() const { return name_; }
        const NamespaceInfo& getNameInfo() const { return nameInfo_; }
        int getConsistencyState() const { return consistency_; }
        unsigned int getRtxNum() const { return nRtx_; }
        int getRtxNum(const ndn::Name& segmentName);
        bool hasOriginalSegments() const { return hasOriginalSegments_; }
        size_t getFetchedNum() const { return fetched_.size(); }
        void toggleLock();
        bool hasAllSegmentsFetched() const { return nDataSegments_+nParitySegments_ == fetched_.size(); }
        int64_t getAssemblingTime() const
        { return ( state_ >= Ready ? assembledTimeUsec_-firstSegmentTimeUsec_ : 0); }
        int64_t getShortestDrd() const
        { return (state_ >= Assembling ? firstSegmentTimeUsec_-requestTimeUsec_ : 0); }
        int64_t getLongestDrd() const
        { return (state_ >= Ready ? assembledTimeUsec_ - requestTimeUsec_ : 0); }
        
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
        friend SampleValidator;
        friend ManifestValidator;
        friend Buffer;

        ndn::Name name_;
        NamespaceInfo nameInfo_;
        std::map<ndn::Name, boost::shared_ptr<SlotSegment>> requested_, fetched_;
        boost::shared_ptr<SlotSegment> lastFetched_;
        unsigned int consistency_, nRtx_, assembledSize_;
        unsigned int nDataSegments_, nParitySegments_;
        bool hasOriginalSegments_;
        State state_;
        int64_t requestTimeUsec_, firstSegmentTimeUsec_, assembledTimeUsec_;
        double assembled_, asmLevel_;
        mutable boost::shared_ptr<Manifest> manifest_;
        mutable Verification verified_;

        virtual void updateConsistencyState(const boost::shared_ptr<SlotSegment>& segment);
        void updateAssembledLevel();
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

    typedef struct _BufferReceipt {
        boost::shared_ptr<const BufferSlot> slot_;
        boost::shared_ptr<const SlotSegment> segment_;
        BufferSlot::State oldState_;
    } BufferReceipt;

    class IBuffer {
    public:
        virtual void reset() = 0;
        virtual bool requested(const std::vector<boost::shared_ptr<const ndn::Interest>>&) = 0;
        virtual BufferReceipt received(const boost::shared_ptr<WireSegment>&) = 0;
        virtual bool isRequested(const boost::shared_ptr<WireSegment>&) const = 0;
        virtual unsigned int getSlotsNum(const ndn::Name&, int) const = 0;
        virtual std::string shortdump() const = 0;
        virtual void attach(IBufferObserver* observer) = 0;
        virtual void detach(IBufferObserver* observer) = 0;
    };

    class Buffer : public NdnRtcComponent, public IBuffer {
    public:
        Buffer(boost::shared_ptr<statistics::StatisticsStorage> storage,
               boost::shared_ptr<SlotPool> pool =
                boost::shared_ptr<SlotPool>(new SlotPool()));

        void reset();

        bool requested(const std::vector<boost::shared_ptr<const ndn::Interest>>&);
        BufferReceipt received(const boost::shared_ptr<WireSegment>& segment);
        bool isRequested(const boost::shared_ptr<WireSegment>& segment) const;
        unsigned int getSlotsNum(const ndn::Name& prefix, int stateMask) const;

        void attach(IBufferObserver* observer);
        void detach(IBufferObserver* observer);
        boost::shared_ptr<SlotPool> getPool() const { return pool_; }

        std::string
        dump() const;
        
    private:
        friend PlaybackQueue;

        mutable boost::recursive_mutex mutex_;
        boost::shared_ptr<SlotPool> pool_;
        std::map<ndn::Name, boost::shared_ptr<BufferSlot>> activeSlots_, reservedSlots_;
        std::vector<IBufferObserver*> observers_;
        boost::shared_ptr<statistics::StatisticsStorage> sstorage_;
        
        std::string
        shortdump() const;

        void 
        dumpSlotDictionary(std::stringstream&, 
            const std::map<ndn::Name, boost::shared_ptr<BufferSlot>> &) const;
        
        void invalidate(const ndn::Name& slotPrefix);
        void invalidatePrevious(const ndn::Name& slotPrefix);
        
        void reserveSlot(const boost::shared_ptr<const BufferSlot>& slot);
        void releaseSlot(const boost::shared_ptr<const BufferSlot>& slot);
    };

    class IBufferObserver {
    public:
        virtual void onNewRequest(const boost::shared_ptr<BufferSlot>&) = 0;
        virtual void onNewData(const BufferReceipt& receipt) = 0;
        virtual void onReset() = 0;
    };

    //******************************************************************************
    typedef boost::function<void(const boost::shared_ptr<const BufferSlot>& slot, double playTimeMs)> ExtractSlot;
    class IPlaybackQueueObserver;

    class IPlaybackQueue {
    public:
        virtual void pop(ExtractSlot) = 0;
        virtual int64_t size() const = 0;
        virtual int64_t pendingSize() const = 0;
        virtual double sampleRate() const = 0;
        virtual double samplePeriod() const = 0;
        virtual void attach(IPlaybackQueueObserver*) = 0;
        virtual void detach(IPlaybackQueueObserver*) = 0;
    };

    /**
     * Class PaybackQueue implements functionality for ordering assembled frames
     * in playback order and provides interface for extracting media samples
     * for playback
     */
    class PlaybackQueue : public NdnRtcComponent,
                          public IPlaybackQueue,
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
        boost::shared_ptr<statistics::StatisticsStorage> sstorage_;

        virtual void onNewRequest(const boost::shared_ptr<BufferSlot>&);
        virtual void onNewData(const BufferReceipt& receipt);
        virtual void onReset();
    };

    class IPlaybackQueueObserver
    {
    public:
        virtual void onNewSampleReady() = 0;
    };
}

#endif /* defined(__ndnrtc__frame_buffer__) */
