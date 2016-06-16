//
//  pipeliner.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__pipeliner__
#define __ndnrtc__pipeliner__

#include <boost/thread/mutex.hpp>

#include "ndnrtc-object.h"
#include "name-components.h"

namespace ndnrtc {
    class SampleEstimator;
    class IBuffer;
    class IInterestControl;
    class IInterestQueue;
    class IPlaybackQueue;
    class ISegmentController;
    class DeadlinePriority;

    typedef struct _PipelinerSettings {
        unsigned int interestLifetimeMs_;
        boost::shared_ptr<SampleEstimator> sampleEstimator_;
        boost::shared_ptr<IBuffer> buffer_;
        boost::shared_ptr<IInterestControl> interestControl_;
        boost::shared_ptr<IInterestQueue> interestQueue_;
        boost::shared_ptr<IPlaybackQueue> playbackQueue_;
        boost::shared_ptr<ISegmentController> segmentController_;
    } PipelinerSettings;

    class IPipeliner {
    public:
        virtual void express(const ndn::Name& threadPrefix, bool placeInBuffer = false) = 0;
        virtual void express(const std::vector<boost::shared_ptr<const ndn::Interest>>&, 
            bool placeInBuffer = false) = 0;
        virtual void segmentArrived(const ndn::Name&) = 0;
        virtual void reset() = 0;
        virtual void setNeedSample(SampleClass cls) = 0;
        virtual void setNeedRightmost() = 0;
        virtual void setSequenceNumber(PacketNumber seqNo, SampleClass cls) = 0;
    };

    /**
     * Pipeliner class implements functionality for expressing Interests towards
     * samples of specified media thread.
     * Pipeliner queries SampleEstimator in order to calculate size of Interest
     * batch to express.
     */
    class Pipeliner : public NdnRtcComponent, public IPipeliner 
    {
    public:
        Pipeliner(const PipelinerSettings& settings);

        /**
         * Express interests for the last requested sample.
         * For instance, if pipeliner previously expressed Interests for sample 100,
         * this expresses Interests for sample 100 again.
         * NOTE: if pipeliner did not express any interests before, this expresses
         * Interest with RightmostChild selector.
         * @param threadPrefix Thread prefix used for Interests
         * @param placeInBuffer Indicates whther interests need to be placed in 
         *          the buffer (does not affect rightmost Interest).
         */
        void express(const ndn::Name& threadPrefix, bool placeInBuffer = false);

        /**
         * Express specified Interests.
         * @param interests Interests to be expressed
         * @param placeInBuffer Indicates whether interests need to be placed in 
         *          the buffer.
         */
        void express(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests,
            bool placeInBuffer = false);

        /**
         * Called each time new segment arrives. 
         * This will query InterestControl for the room size and if it's not zero
         * or negative, will continuously issue Interest batches towards new 
         * samples according to he current sample class priority unless 
         * InterestControl will tell that there is no room for more Interests.
         * This also places issued Interests in the buffer by calling requested()
         * method.
         * @see InterestControl
         * @see Buffer::requested()
         */
        void segmentArrived(const ndn::Name& threadPrefix);
        void reset();

        /**
         * Sets priority for the next sample. After calling this method, pipeliner
         * will prioritize specified sample class for the next batch of Interests
         * @param cls Sample class (Delta, Key, etc.)
         */
        void setNeedSample(SampleClass cls) { nextSamplePriority_ = cls; }

        /**
         * Sets flag to request rightmost interest on next express(const ndn::Name&)
         * invocation
         * @see express(const ndn::Name&)
         */
        void setNeedRightmost() { lastRequestedSample_ = SampleClass::Unknown; }

        /**
         * Sets pipeliner's frame sequence number counters
         * @param seqNo Sequence number 
         * @param cls Frame class (Key or Delta)
         */
        void setSequenceNumber(PacketNumber seqNo, SampleClass cls);

    private:
        typedef struct _SequenceCounter {
            PacketNumber delta_, key_;
        } SequenceCounter;

        unsigned int interestLifetime_;
        boost::shared_ptr<SampleEstimator> sampleEstimator_;
        boost::shared_ptr<IBuffer> buffer_;
        boost::shared_ptr<IInterestControl> interestControl_;
        boost::shared_ptr<IInterestQueue> interestQueue_;
        boost::shared_ptr<IPlaybackQueue> playbackQueue_;
        boost::shared_ptr<ISegmentController> segmentController_;
        SequenceCounter seqCounter_;
        SampleClass nextSamplePriority_, lastRequestedSample_;

        void request(const std::vector<boost::shared_ptr<const ndn::Interest>>& interests,
            const boost::shared_ptr<DeadlinePriority>& prioirty);
        void request(const boost::shared_ptr<const ndn::Interest>& interest,
            const boost::shared_ptr<DeadlinePriority>& prioirty);
        std::vector<boost::shared_ptr<const ndn::Interest>>
        getBatch(ndn::Name n, SampleClass cls,
            bool noParity = false) const;
    };
}

#endif /* defined(__ndnrtc__pipeliner__) */
