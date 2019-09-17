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

#define BOOST_BIND_NO_PLACEHOLDERS

#include <mutex>
#include <set>

#include <boost/signals2.hpp>
#include <boost/asio.hpp>
#include <ndn-cpp/name.hpp>

#include "ndnrtc-common.hpp"
#include "name-components.hpp"

#include "pool.hpp"
#include "video-codec.hpp"

#include "slot-buffer.hpp"
#include "statistics.hpp"
#include "ndnrtc-object.hpp"
#include "pipeline.hpp"
#include "estimators.hpp"

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

    namespace packets {
        class Meta;
        class Manifest;
    }

    struct _CommonHeader;
    class WireSegment;

    class SlotSegment {
    public:

        SlotSegment(const std::shared_ptr<const ndn::Interest>&);

        const NamespaceInfo& getInfo() const;
        void setData(const std::shared_ptr<WireSegment>& data);
        const std::shared_ptr<WireSegment>& getData() const { return data_; }

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
        std::shared_ptr<const ndn::Interest> getInterest() const { return interest_; }

        /**
         * Takes into account if the segment is original or not.
         * If the segment is original, this returns getRoundTripDelayUsec() minus
         * generation delay received in metadata for the segment.
         */
        int64_t getDrdUsec() const;
        int64_t getDgen() const;

    private:
        std::shared_ptr<const ndn::Interest> interest_;
        NamespaceInfo interestInfo_;
        std::shared_ptr<WireSegment> data_;
        int64_t requestTimeUsec_, arrivalTimeUsec_;
        size_t requestNo_;
        bool isVerified_;
    } DEPRECATED;

    //******************************************************************************
    class VideoFrameSlot;
    class AudioBundleSlot;
    class Buffer;
    class Manifest;
    class SampleValidator;
    class ManifestValidator;
    class RequestQueue;

    class BufferSlot : public IPoolObject
                     , public IPipelineSlot
                     , public std::enable_shared_from_this<BufferSlot>
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

        PipelineSlotState getState() const override { return slotState_; }
        void setRequests(const std::vector<std::shared_ptr<DataRequest>>&) override;
        const std::vector<std::shared_ptr<DataRequest>>& getRequests() const;

        const ndn::Name& getPrefix() const override  { return name_; }
        SlotTriggerConnection subscribe(PipelineSlotState, OnSlotStateUpdate) override;
        NeedDataTriggerConnection addOnNeedData(OnNeedData) override;

        /**
         * Clears all internal structures of this slot and returns to Free state
         * as if slot has just been created. No memory deallocation/reallocation is
         * performed, thus operation is not expensive.
         */
        void clear() override;
        bool isReadyForDecoder() const;

        int64_t getAssemblingTime() const
        { return ( slotState_ >= PipelineSlotState::Assembling ? lastDataTsUsec_ - firstDataTsUsec_ : 0); }
        int64_t getShortestDrd() const
        { return (slotState_ >= PipelineSlotState::Assembling ? firstDataTsUsec_-firstRequestTsUsec_ : 0); }
        int64_t getLongestDrd() const
        { return (slotState_ == PipelineSlotState::Ready ? lastDataTsUsec_ - firstRequestTsUsec_ : 0); }

        uint64_t getFirstRequestTsUsec() const { return (uint64_t)firstRequestTsUsec_; }
        uint64_t getFirstDataTsUsec() const { return (uint64_t)firstDataTsUsec_; }
        uint64_t getLastDataTsUsec() const { return (uint64_t)lastDataTsUsec_;}

        Verification getVerificationStatus() const { return verified_; }
        const NamespaceInfo& getNameInfo() const { return nameInfo_; }
        double getFetchProgress() const { return fetchProgress_; }

        std::string
        dump(bool showLastSegment = false) const;

        std::shared_ptr<const packets::Meta> getMetaPacket() const { return meta_; }

        size_t getDataSegmentsNum() const { return nDataSegments_; }
        size_t getFetchedDataSegmentsNum() const { return nDataSegmentsFetched_; }
        size_t getParitySegmentsNum() const { return nParitySegments_; }
        size_t getFetchedParitySegmentsNum() const { return nParitySegmentsFetched_; }
        size_t getFetchedBytesTotal() const { return fetchedBytesTotal_; }
        size_t getFetchedBytesData() const { return fetchedBytesData_; }
        size_t getFetchedBytesParity() const { return fetchedBytesParity_; }

    private:
        PipelineSlotState slotState_;
        std::vector<std::shared_ptr<DataRequest>> requests_;
        std::vector<RequestTriggerConnection> requestConnections_;
        SlotTrigger onPending_, onAssembling_, onReady_, onUnfetchable_;
        NeedDataTrigger onMissing_;
        bool metaIsFetched_, manifestIsFetched_;
        std::shared_ptr<const packets::Meta> meta_;
        std::shared_ptr<const packets::Manifest> manifest_;
        SegmentNumber maxDataSegNo_, maxParitySegNo_;

        int64_t firstRequestTsUsec_, firstDataTsUsec_, lastDataTsUsec_;
        size_t nDataSegments_, nParitySegments_;
        size_t nDataSegmentsFetched_, nParitySegmentsFetched_;
        size_t fetchedBytesData_, fetchedBytesParity_, fetchedBytesTotal_;
        double fetchProgress_;

        void onReply(const DataRequest&);
        void onError(const DataRequest&);

        void checkForMissingSegments(const DataRequest&);
        void updateAssemblingProgress(const DataRequest&);
        void triggerEvent(PipelineSlotState, const DataRequest&);

        // ----------------------------------------------------------------------------------------
        // CODE BELOW IS DEPRECATED
    public:
        int getConsistencyState() const { return consistency_; }
        double getAssembledLevel() const { return fetchProgress_; }

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
        segmentsRequested(const std::vector<std::shared_ptr<const ndn::Interest>>& interests) DEPRECATED;

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
        const std::shared_ptr<SlotSegment>
        segmentReceived(const std::shared_ptr<WireSegment>& segment) DEPRECATED;

        /**
         * Returns an array of names of missing segments
         */
        std::vector<ndn::Name> getMissingSegments() const DEPRECATED;

        /**
         * Returns an array of pending Interests for this slot
         */
        const std::vector<std::shared_ptr<const ndn::Interest>> getPendingInterests() const DEPRECATED;

        /**
         *
         */
        const std::vector<std::shared_ptr<const SlotSegment>> getFetchedSegments() const DEPRECATED;


//        State getState(int phony = 0) const DEPRECATED { return state_; }

        unsigned int getRtxNum() const DEPRECATED { return nRtx_; }
        int getRtxNum(const ndn::Name& segmentName) DEPRECATED;
        bool hasOriginalSegments() const DEPRECATED { return hasOriginalSegments_; }
        size_t getFetchedNum() const DEPRECATED { return fetched_.size(); }
        void toggleLock() DEPRECATED;
        bool hasAllSegmentsFetched() const { return nDataSegments_+nParitySegments_ == fetched_.size(); }

        /**
         * Returns common packet header if it's available (HeaderMeta consistency),
         * otherwise throws an error.
         * @return CommonHeader structure
         * @see CommonSamplePacket
         */
        const _CommonHeader getHeader() const DEPRECATED;

    private:


        // ???
//        std::map<ndn::Name, std::shared_ptr<DataRequest>> expressed_;
//        std::map<ndn::Name, std::shared_ptr<DataRequest>> fetched_;
//        std::map<ndn::Name, std::shared_ptr<DataRequest>> nacked_;
//        std::map<ndn::Name, std::shared_ptr<DataRequest>> appNacked_;
//        std::map<ndn::Name, std::shared_ptr<DataRequest>> timedout_;

        // ----------------------------------------------------------------------------------------
        // CODE BELOW IS DEPRECATED
        size_t assembledBytes_;
        double assembledPct_, asmLevel_;

        friend VideoFrameSlot;
        friend AudioBundleSlot;
        friend SampleValidator;
        friend ManifestValidator;
        friend Buffer;

        ndn::Name name_;
        NamespaceInfo nameInfo_;
        std::map<ndn::Name, std::shared_ptr<SlotSegment>> requested_, fetched_;
        std::shared_ptr<SlotSegment> lastFetched_;
        unsigned int consistency_, nRtx_, assembledSize_;

        bool hasOriginalSegments_;
        double assembled_;

        State state_;


        int64_t requestTimeUsec_, firstSegmentTimeUsec_, assembledTimeUsec_;
//        double assembled_, asmLevel_;
//        mutable std::shared_ptr<Manifest> manifest_;
        mutable Verification verified_;

        virtual void updateConsistencyState(const std::shared_ptr<SlotSegment>& segment);
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
        std::shared_ptr<ImmutableFrameAlias>
        readPacket(const BufferSlot& slot, bool& recovered);

        _VideoFrameSegmentHeader
        readSegmentHeader(const BufferSlot& slot);

    private:
        std::shared_ptr<std::vector<uint8_t>> storage_;
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
        std::shared_ptr<ImmutableAudioBundleAlias>
        readBundle(const BufferSlot& slot);

    private:
        std::shared_ptr<std::vector<uint8_t>> storage_;
    };

    //******************************************************************************
    class SlotPool {
    public:
        SlotPool(const size_t& capacity = 300);

        std::shared_ptr<BufferSlot> pop();
        bool push(const std::shared_ptr<BufferSlot>& slot);

        size_t capacity() const { return capacity_; }
        size_t size() const { return pool_.size(); }

    private:
        SlotPool(const SlotPool&) = delete;

        size_t capacity_;
        std::vector<std::shared_ptr<BufferSlot>> pool_;
    };

    //******************************************************************************
    class DecodeQueue;
    class IBufferObserver;
    class PlaybackQueue;

    typedef struct _BufferReceipt {
        std::shared_ptr<const BufferSlot> slot_;
        std::shared_ptr<const SlotSegment> segment_;
        BufferSlot::State oldState_;
    } BufferReceipt;

    class IBuffer {
    public:
        virtual void reset() = 0;
        virtual bool requested(const std::vector<std::shared_ptr<const ndn::Interest>>&) = 0;
        virtual BufferReceipt received(const std::shared_ptr<WireSegment>&) = 0;
        virtual bool isRequested(const std::shared_ptr<WireSegment>&) const = 0;
        virtual unsigned int getSlotsNum(const ndn::Name&, int) const = 0;
        virtual std::string shortdump() const = 0;
        virtual void attach(IBufferObserver* observer) = 0;
        virtual void detach(IBufferObserver* observer) = 0;
    };

    typedef boost::signals2::signal<void(const std::shared_ptr<BufferSlot>&)> BufferSlotUpdateTrigger;
    typedef BufferSlotUpdateTrigger OnSlotUnfetchable;
    typedef BufferSlotUpdateTrigger OnSlotReady;
    typedef BufferSlotUpdateTrigger OnSlotDiscard;

    class Buffer : public NdnRtcComponent, public IBuffer {
    public:
        Buffer(std::shared_ptr<RequestQueue> interestQ,
               std::shared_ptr<DecodeQueue> decodeQ,
               uint64_t slotRetainIntervalUsec = 3E6,
               std::shared_ptr<statistics::StatisticsStorage> storage =
                std::shared_ptr<statistics::StatisticsStorage>(statistics::StatisticsStorage::createConsumerStatistics()));

        void newSlot(std::shared_ptr<IPipelineSlot>);
        void removeSlot(const PacketNumber&);
        double getDelayEstimate() const { return delayEstimate_; }

        bool getIsJitterCompensationOn() const { return isJitterCompensationOn_; }
        void setIsJitterCompensationOn(bool jitterCompensation) { isJitterCompensationOn_ = jitterCompensation; }

        OnSlotUnfetchable onSlotUnfetchable;
        OnSlotReady onSlotReady;
        // signal which will be called when an old undecodable slot is discarded
        // if slot stays in the buffer for longer than slotRetainInterval and is not
        // ready for decoding, it will be discarded
        // this signal should be used for recylcing slots (i.e. returning them to the pool)
        OnSlotDiscard onSlotDiscard;

        void reset();

        std::string
        dump() const;

        // DEPRECATED CODE BLOCK START

        bool requested(const std::vector<std::shared_ptr<const ndn::Interest>>&);
        BufferReceipt received(const std::shared_ptr<WireSegment>& segment);
        bool isRequested(const std::shared_ptr<WireSegment>& segment) const;
        unsigned int getSlotsNum(const ndn::Name& prefix, int stateMask) const;

        void attach(IBufferObserver* observer);
        void detach(IBufferObserver* observer);
        std::shared_ptr<SlotPool> getPool() const { return pool_; }

        // DEPRECATED CODE BLOCK END
    private:
        typedef struct _SlotEntry {
            std::shared_ptr<BufferSlot> slot_;
            NeedDataTriggerConnection onMissingDataConn_;
            SlotTriggerConnection onReadyConn_, onUnfetchableConn_, onAssemblingConn_;
            uint64_t insertedUsec_, pushDeadlineUsec_;
            bool pushedForDecode_;
        } SlotEntry;

        // jitter buffer delay is calculated according to the formula:
        //      B(i) = Dqav(i) + gamma * Jitter
        //  where Jitter is a network jitter estimation from RequestQueue and
        //  Dqav:
        //      Dqav(i) = theta * Dqav(i-1) + (1-theta) * Dq(i)
        //          where Dq(i) -- i-th frame re-assembly delay
        //
        // ??? -- why use network jitter instead of Dq jitter?
        // TODO: try running estimation when using Dq jitter
        double delayEstimateGamma_, delayEstimateTheta_, delayEstimate_;
        estimators::Filter dqFilter_;
        bool isJitterCompensationOn_;
        uint64_t cleanupIntervalUsec_, lastCleanupUsec_, slotRetainIntervalUsec_;

        std::map<PacketNumber, SlotEntry> slots_;
        std::map<int32_t, PacketNumber> gopDecodeMap_;
        PacketNumber lastPushedSampleNo_;
        std::shared_ptr<BufferSlot> lastPushedSlot_;

        std::shared_ptr<statistics::StatisticsStorage> sstorage_;
        std::shared_ptr<RequestQueue> requestQ_;
        std::shared_ptr<DecodeQueue> decodeQ_;
        boost::asio::steady_timer slotPushTimer_;
        uint64_t slotPushFireTime_;
        int64_t slotPushDelay_;

        std::string
        shortdump() const;

        void
        calculateDelay(double dQ);
        void
        checkReadySlots(uint64_t);
        void
        doCleanup(uint64_t);
        void
        setSlotPushDeadline(const std::shared_ptr<BufferSlot>&, uint64_t ts);
        void
        setupPushTimer(uint64_t);

        // CODE BELOW IS DEPRECATED
        friend PlaybackQueue;

        mutable std::recursive_mutex mutex_;
        std::shared_ptr<SlotPool> pool_;
        std::map<ndn::Name, std::shared_ptr<BufferSlot>> activeSlots_, reservedSlots_;
        std::vector<IBufferObserver*> observers_;

        void
        dumpSlotDictionary(std::stringstream&,
            const std::map<ndn::Name, std::shared_ptr<BufferSlot>> &) const;

        void invalidate(const ndn::Name& slotPrefix);
        void invalidatePrevious(const ndn::Name& slotPrefix);

        void reserveSlot(const std::shared_ptr<const BufferSlot>& slot);
        void releaseSlot(const std::shared_ptr<const BufferSlot>& slot);
    };

    class IBufferObserver {
    public:
        virtual void onNewRequest(const std::shared_ptr<BufferSlot>&) = 0;
        virtual void onNewData(const BufferReceipt& receipt) = 0;
        virtual void onReset() = 0;
    };

    //******************************************************************************

    class DecodeQueue : public NdnRtcComponent
    {
    public:
        /**
         * DecodeQueue
         */
        DecodeQueue(boost::asio::io_service&, uint32_t size = 150,
                    uint32_t capacity = 160);
        ~DecodeQueue(){}

        // signal which will be called when an slot is discarded
        // decode queue keeps capacity number of slots, discarding
        // old slots. when this happens, this signal will be triggered for
        // every slot in discarded GOP
        OnSlotDiscard onSlotDiscard;

        void push(const std::shared_ptr<const BufferSlot>&);
//        PacketNumber getCurrentPlaybackPointer() const { return pbPointer_; };
        size_t getSize() const { return frames_.size(); }

        /**
         * Get access to decoded frame that is currently pointed by playback
         * pointer and advance playback pointer by the specified step. This does
         * not remove frame from the queue.
         * @param step Step (number of frames) by which to advance playback
         *      pointer after frame retrieval. Negative step advances playback
         *      pointer backwards.
         * @param allowDecoding If true, will attempt to decode frame if current frame
         *      is not decoded.
         * @return Returns valid VideoCodec::Image, if requested frame can be
         *      returned (frame is either already decoded or will be decoded
         *      during this call). If frame can not be retrieved, or decoded
         *      (if allowDecoding==false),
         *      returns null VideoCodec::Image. Client code should check for
         *      "image.getAllocatedSize() == 0" to check validity.
         *      Returned image must be discared after manipulation or saved
         *      into a file/memory. The validity of the image is  guaranteed
         *      until the next call to this method.
         */
        const VideoCodec::Image& get(int32_t step = 0, bool allowDecoding = true);

        /**
         * Advances playback pointer by specified number of frames.
         * @param step Number of frames by which current playback pointer will
         *          be advanced. To get to the most recent frame (end of the
         *          queue), pass getSize() as an argument. To get to the oldest
         *          frame (beginning of the queue), pass -getSize() as an
         *          argument.
         * @return Actual number of frames the pointer was advanced.
         */
        int32_t seek(int32_t step);

        /**
         * Dumps contents of DecodeQueue into a string.
         */
        std::string dump() const;
    protected:
        class BitstreamData : public IPoolObject {
        public:
            BitstreamData(size_t reservedBytes = 32768)
            {
                bitstreamData_.reserve(reservedBytes);
                fecList_.reserve(256);
            }

            void clear()
            {
                slot_.reset();
                memset(&encodedFrame_, 0, sizeof(encodedFrame_));
                memset(bitstreamData_.data(), 0, bitstreamData_.capacity());
                memset(fecList_.data(), 0, fecList_.capacity());
            }

            const EncodedFrame& loadFrom(std::shared_ptr<const BufferSlot>&,
                bool& recovered);
        private:
            EncodedFrame encodedFrame_;
            std::shared_ptr<const BufferSlot> slot_;
            std::vector<uint8_t> bitstreamData_;
            std::vector<uint8_t> fecList_;
        };

        class VpxExternalBufferList : public IFrameBufferList, public NdnRtcComponent {
        public:
            class DecodedData : public IPoolObject {
            public:
                DecodedData(size_t reservedBytes = (2 << 20))
                {
                    rawData_.reserve(reservedBytes);
                    clear();
                }

                void clear();
                void memset(uint8_t c);
                bool hasImage() const { return rawData_.size(); }
                void setImage(const VideoCodec::Image& img, const NamespaceInfo&);
                const VideoCodec::Image& getImage() const;
                bool getIsInUseByDecoder() const { return inUseByDecoder_; }

           protected:
                friend VpxExternalBufferList;

                uint8_t *getData() const { return const_cast<uint8_t*>(rawData_.data()); }
                size_t getSize() const { return rawData_.size(); }
                size_t getCapacity() const { return rawData_.capacity(); }
                void reserve(size_t nBytes) { rawData_.reserve(nBytes); }
                void resize(size_t length) { rawData_.resize(length); }

                bool inUseByDecoder_;

            private:

                NamespaceInfo ni_;
                std::vector<uint8_t> rawData_;
                std::shared_ptr<VideoCodec::Image> img_;
            };

            VpxExternalBufferList(uint32_t capacity);

            void recycle(DecodedData *dd);

            // IFrameBufferList interface
            int getFreeFrameBuffer(size_t minSize, vpx_codec_frame_buffer_t *fb);
            int returnFrameBuffer(vpx_codec_frame_buffer_t *fb);
            size_t getUsedBufferNum();
            size_t getFreeBufferNum();

        private:
            Pool<DecodedData> framePool_;
            std::vector<std::shared_ptr<DecodedData>> dataInUse_;
            std::vector<std::shared_ptr<DecodedData>> delayedPurgeList_;

            std::shared_ptr<DecodedData> getPointer(DecodedData*);
            void doCleanup();
        };

    private:
        static std::shared_ptr<const VideoCodec::Image> ZeroImage;
        typedef struct _FrameQueueEntry {
            bool operator<(const _FrameQueueEntry& sample) const
            { return slot_->getNameInfo().sampleNo_ < sample.slot_->getNameInfo().sampleNo_; }

            std::shared_ptr<const BufferSlot> slot_;
            mutable std::shared_ptr<BitstreamData> bitstreamData_;
            mutable VpxExternalBufferList::DecodedData* rawData_; // owned by the frame buffer pool
        } FrameQueueEntry;
        typedef std::set<FrameQueueEntry> FrameQueue;
        typedef struct _CodecContext {
            std::recursive_mutex mtx_;
            VideoCodec codec_;
            FrameQueue::iterator lastDecoded_;
            FrameQueue::iterator gopStart_, gopEnd_;

            bool isGopComplete() const { return lastDecoded_ == gopEnd_; }
        } CodecContext;
        typedef struct _GopEntry {
            int32_t no_;
            uint32_t nDecoded_;
            FrameQueue::iterator start_, end_;
        } GopEntry;

        boost::asio::io_service &io_;
        uint32_t size_;
        FrameQueue frames_;
        std::map<int32_t, GopEntry> gops_;
        CodecContext codecContext_;

        // lock hierarchy: masterMtx_ -> playbackMtx_ -> codecContext_.mtx_
        std::recursive_mutex masterMtx_;
        std::recursive_mutex playbackMtx_;
        FrameQueue::iterator pbPointer_; // current playback pointer
        int32_t lastAsked_; // last asked frame
        FrameQueue::iterator lastRetrieved_;

        VpxExternalBufferList::DecodedData *lockedRecycleData_;
        int32_t discardGop_;

        Pool<BitstreamData> bitstreamDataPool_;
        VpxExternalBufferList vpxFbList_;
        std::atomic<bool> isCheckFramesScheduled_;
        // checks current frames and runs decoder on frames that can be decoded
        void checkFrames();

        /**
         * Runs decoder on multiple frames. Decoding may halt if there's no free memory left
         * for decoded frames or time limit is reached.
         * @param ctx Codec context. Updated as decoding progresses.
         * @param start Iterator to a start frame with which decoding will start.
         * @param nToDecode Number of frames to decode. If all frames decode, lastDecoded_ will
         *                  point to (start + nToDecode) frame.
         * @param timeLimitMs Time limit for decoding. 0 means no time limit. Decoding many
         *                    frames in a row may take considerable time. If timeLimit is
         *                    specified, this function will return when limit is reached. One
         *                    must compare ctx.lastDecoded_ and start in order to get number of
         *                    frames actually decoded.
         */
        void runDecoder(CodecContext& ctx, FrameQueue::iterator start, int32_t nToDecode,
                        uint32_t timeLimitMs = 0);
        void resetCodecContext();
        void doCleanup();
        bool frameIteratorWithinRange(const FrameQueue::iterator&, const FrameQueue::iterator&,
                                      const FrameQueue::iterator&);

        int32_t getEncodedNum(const GopEntry& ge);
      
        /**
         * Finds encoded or decoded frame in frames_ that is closest to the iterator in a given
         * direction.
         * @param p             Iterator from which search will start
         * @param encoded       If true, checks for encoded frames, otherwise -- decoded frames
         * @param directionFwd  If true, search in forward direction, otherwise -- backward
         *                      direction
         * @return  Returns an increment number which can advance p to the found iterator.
         */
        int32_t getClosest(const FrameQueue::iterator &p, bool encoded, bool directionFwd)
        {
            FrameQueue::iterator it = p;
            while (it != frames_.end())
            {
                if (it != p)
                {
                    if (encoded && !it->rawData_) break;
                    if (!encoded && it->rawData_) break;
                }

                if (it == frames_.begin())
                    it = frames_.end();
                else
                    it = (directionFwd ? std::next(it) : std::prev(it));
            }

            if (it == frames_.end()) return 0;

            return directionFwd ? distance(p, it) : -distance(it, p);
        }

        // gets an offset for the farthest encoded/decoded frame from the frame pointed by
        // the given iterator
        // search begins at the given iterator and progresses in the given direction (forward
        // or backward) until frame of the opposite state is found (i.e. !encoded)
        int32_t getFarthest(const FrameQueue::iterator &p, bool encoded, bool directionFwd)
        {
            FrameQueue::iterator it = p;
            FrameQueue::iterator found = it;
            
            while (it != frames_.end() && (it->rawData_ && found->rawData_)^encoded )
            {
                if (it != p)
                {
                    if (encoded^(bool)it->rawData_) found = it;
                }

                if (it == frames_.begin())
                    it = frames_.end();
                else
                    it = (directionFwd ? std::next(it) : std::prev(it));
            }
            
            return directionFwd ? distance(p, found) : -distance(found, p);
        }

        inline bool checkAssignMemory(FrameQueue::iterator& it)
        {
            if (!it->bitstreamData_)
            {
                if (!it->bitstreamData_)
                    it->bitstreamData_ = bitstreamDataPool_.pop();
            }

            return (it->bitstreamData_.get() != nullptr);
        }
    };

    typedef std::function<void(const std::shared_ptr<const BufferSlot>& slot, double playTimeMs)> ExtractSlot;
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
            const std::shared_ptr<Buffer>& buffer);
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
            Sample(const std::shared_ptr<const BufferSlot>& slot):slot_(slot){}

            std::shared_ptr<const BufferSlot> slot() const { return slot_; }
            int64_t timestamp() const;
            bool operator<(const Sample& sample) const
            { return this->timestamp() < sample.timestamp(); }

        private:
            std::shared_ptr<const BufferSlot> slot_;
        };

        mutable std::recursive_mutex mutex_;
        ndn::Name streamPrefix_;
        std::shared_ptr<Buffer> buffer_;
        double packetRate_;
        std::set<Sample> queue_;
        std::vector<IPlaybackQueueObserver*> observers_;
        std::shared_ptr<statistics::StatisticsStorage> sstorage_;

        virtual void onNewRequest(const std::shared_ptr<BufferSlot>&);
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
