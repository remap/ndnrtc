//
// latency-control.hpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __latency_control_h__
#define __latency_control_h__

#include <boost/thread/mutex.hpp>

#include "drd-estimator.hpp"
#include "segment-controller.hpp"
#include "ndnrtc-object.hpp"
#include "buffer-control.hpp"

namespace ndnlog
{
namespace new_api
{
class ILogger;
}
}

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

namespace estimators
{
class Average;
}

class StabilityEstimator;
class DrdChangeEstimator;
class ILatencyControlObserver;
class IPlayoutControl;

typedef enum _PipelineAdjust {
    IncreasePipeline,
    DecreasePipeline,
    KeepPipeline
} PipelineAdjust;

class ILatencyControl
{
  public:
    virtual void reset() = 0;
    virtual PipelineAdjust getCurrentCommand() const = 0;
    virtual void registerObserver(ILatencyControlObserver *o) = 0;
    virtual void unregisterObserver() = 0;
    virtual void setPlayoutControl(boost::shared_ptr<IPlayoutControl> playoutControl) = 0;
};

/**
 * Latency control runs estimation of latest data arrival and detects
 * changes in DRD when pipeline size changes. In order for it to work 
 * properly, one should call frameArrived() each time first segment of
 * new sample has arrived (regardless of segment number) and 
 * setTargetRate() whenever new information about target sample rate 
 * has became available or updated.
 */
class LatencyControl : public NdnRtcComponent,
                       public ILatencyControl,
                       public IDrdEstimatorObserver,
                       public IBufferControlObserver
{
  public:
    LatencyControl(unsigned int timeoutWindowMs,
                   const boost::shared_ptr<const DrdEstimator> &drd,
                   const boost::shared_ptr<statistics::StatisticsStorage> &storage);
    ~LatencyControl();

    void onDrdUpdate();
    void onCachedDrdUpdate(double, double) { /*ignored*/}
    void onOriginalDrdUpdate(double, double) { /*ignored*/}

    void targetRateUpdate(double rate) { targetRate_ = rate; }
    void sampleArrived(const PacketNumber &playbackNo);
    void reset();

    void setPlayoutControl(boost::shared_ptr<IPlayoutControl> playoutControl)
    {
        playoutControl_ = playoutControl;
    }

    void registerObserver(ILatencyControlObserver *o);
    void unregisterObserver();

    PipelineAdjust getCurrentCommand() const { return currentCommand_; }

    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

  private:
    class IQueueSizeStrategy
    {
      public:
        virtual unsigned int getTargetPlayoutSize(const estimators::Average &drd, const unsigned int &lowerLimit) = 0;
    };

    class DefaultStrategy : public IQueueSizeStrategy
    {
      public:
        DefaultStrategy(double alpha = 4.) : alpha_(alpha) {}
        unsigned int getTargetPlayoutSize(const estimators::Average &drd, const unsigned int &lowerLimit);

      private:
        double alpha_;
    };

    boost::mutex mutex_;
    boost::shared_ptr<StabilityEstimator> stabilityEstimator_;
    boost::shared_ptr<DrdChangeEstimator> drdChangeEstimator_;
    boost::shared_ptr<IPlayoutControl> playoutControl_;
    boost::shared_ptr<IQueueSizeStrategy> queueSizeStrategy_;
    bool waitForChange_, waitForStability_;
    int64_t timestamp_;
    unsigned int timeoutWindowMs_;
    boost::shared_ptr<const DrdEstimator> drd_;
    estimators::Average interArrival_;
    double targetRate_;
    ILatencyControlObserver *observer_;
    PipelineAdjust currentCommand_;
    boost::shared_ptr<statistics::StatisticsStorage> sstorage_;

    void pipelineChanged(int64_t now);
};

class ILatencyControlObserver
{
  public:
    virtual bool needPipelineAdjustment(const PipelineAdjust &) = 0;
};
}

#endif
