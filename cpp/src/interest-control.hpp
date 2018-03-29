//
// interest-control.hpp
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __interest_control_h__
#define __interest_control_h__

#include <deque>
#include <boost/atomic.hpp>

#include "ndnrtc-common.hpp"
#include "ndnrtc-object.hpp"
#include "drd-estimator.hpp"
#include "buffer-control.hpp"

namespace ndn
{
class Name;
}

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class WireSegment;

class IInterestControl
{
  public:
    virtual void reset() = 0;
    virtual void initialize(double rate, int initPipelineLimit) = 0;
    virtual bool decrement() = 0;
    virtual bool increment() = 0;
    virtual size_t pipelineLimit() const = 0;
    virtual size_t pipelineSize() const = 0;
    virtual int room() const = 0;
    virtual bool burst() = 0;
    virtual bool withhold() = 0;
    virtual void markLowerLimit(unsigned int lowerLimit) = 0;
    virtual std::string snapshot() const = 0;
};

/**
	 * InterestControl implements algorithm for lambda control or Interests 
	 * expression control. It keeps track of the max limit size of the pipeline
	 * of outstanding Interests, current size of the pipeline and size of 
	 * the room for expressing more Interests.
	 */
class InterestControl : public NdnRtcComponent,
                        public IInterestControl,
                        public IDrdEstimatorObserver,
                        public IBufferControlObserver
{
  public:
    static const unsigned int MinPipelineSize;
    /**
     * Interface for Interest pipeline adjustment strategy
     */
    class IStrategy
    {
      public:
        virtual void getLimits(double rate, boost::shared_ptr<DrdEstimator> drdEstimator,
                               unsigned int &lowerLimit, unsigned int &upperLimit) = 0;
        virtual int burst(unsigned int currentLimit,
                          unsigned int lowerLimit, unsigned int upperLimit) = 0;
        virtual int withhold(unsigned int currentLimit,
                             unsigned int lowerLimit, unsigned int upperLimit) = 0;
    };

    /**
     * Default Interest pipeline adjustment strategy:
     *  - set pipeline limit based on current DRD and samplre rate values
     *  - bursts by half of the current limit value
     *  - withholding - decrease to the limit found by binary search
     *		between lower limit and current limit
     */
    class StrategyDefault : public IStrategy
    {
      public:
        void getLimits(double rate, boost::shared_ptr<DrdEstimator> drdEstimator,
                       unsigned int &lowerLimit, unsigned int &upperLimit);
        int burst(unsigned int currentLimit,
                  unsigned int lowerLimit, unsigned int upperLimit);
        int withhold(unsigned int currentLimit,
                     unsigned int lowerLimit, unsigned int upperLimit);
    };

    InterestControl(const boost::shared_ptr<DrdEstimator> &,
                    const boost::shared_ptr<statistics::StatisticsStorage> &storage,
                    boost::shared_ptr<IStrategy> strategy = boost::make_shared<StrategyDefault>());
    ~InterestControl();

    /**
     * Initializes interest control class with given sample rate and initial
     * pipeline size limit (i.e. maximum number of outstanding interests to 
     * be sent out by pipelienr initially).
     */
    void initialize(double rate, int pipelineLimit) override;
    void reset() override;

    /**
     * Should be called every time when counter needs to be decremented,
     * for instance, when previously requested data arrives.
     * @return true if counter was decremented, false if pipeline can't be 
     * 	decremented
     */
    bool decrement() override;

    /**
     * Should be called every time when counter needs to be incremented,
     * for instance, when Interests towards a new sample were issued.
     * @return true if counter was incremented, false if pipeline can't be
     * 	incremented (room is less than or equal zero)
     * @see room
     */
    bool increment() override;

    /**
     * Returns current pipeline maximum size. This size can be changed 
     * by client.
     * @see decrease(), increase()
     */
    size_t pipelineLimit() const override { return limit_; }

    /**
     * Returns current pipeline size, i.e. number of outstanding samples
     */
    size_t pipelineSize() const override { return pipeline_; }

    /**
     * Returns size of the room for expressing more Interests. If room is 
     * zero or negative, no Interests for new samples should be expressed.
     */
    int room() const override { return limit_ - pipeline_; }

    /**
     * Increases current pipeline limit, if possible.
     * @return true if limit was increased, false otherwise
     */
    bool burst() override;

    /**
     * Decreases current pipeline limit, if possible.
     * @return true if decrease was possible, false - otherwise
     */
    bool withhold() override;

    /**
     * Sets lower limit for pipeline size
     */
    void markLowerLimit(unsigned int lowerLimit) override;

    /**
     * Returns symbolic representation of pipeline state
     * For example:
     *      1-8[⬆︎⬆︎⬆︎⬆︎◻︎◻︎◻︎◆]4-8 (4)
     * - first two numbers represent lower limit and upper limit for the pipeline
     * - up arrow (⬆︎) represents outstanding interest
     * - empty slot (◻︎) represents room to express an interest
     * - diamond (◆) represents current pipeline limit
     * - last three numbers:
     *      - current number of outstanding interests (pipeline size)
     *      - current pipeline limit
     *      - current pipeline room size (limit-pipeline)
     */
    std::string snapshot() const override;

    // IDrdEstimatorObserver
    void onDrdUpdate() override;
    void onCachedDrdUpdate() override;
    void onOriginalDrdUpdate() override;

    // IBufferControlObserver
    void targetRateUpdate(double rate) override;
    void sampleArrived(const PacketNumber &) override { decrement(); }

  private:
    boost::shared_ptr<IStrategy> strategy_;
    boost::atomic<bool> initialized_, limitSet_;
    unsigned int lowerLimit_, limit_, upperLimit_;
    boost::atomic<int> pipeline_;
    boost::shared_ptr<DrdEstimator> drdEstimator_;
    double targetRate_;
    boost::shared_ptr<statistics::StatisticsStorage> sstorage_;

    void setLimits();
    void changeLimitTo(unsigned int newLimit);
};
}

#endif