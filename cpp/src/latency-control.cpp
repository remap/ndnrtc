// 
// latency-control.cpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "latency-control.h"
#include <boost/make_shared.hpp>
#include <boost/thread/lock_guard.hpp>

#include "estimators.h"
#include "clock.h"
#include "statistics.h"

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace estimators;

//******************************************************************************
namespace ndnrtc {
	class StabilityEstimator : public ndnlog::new_api::ILoggingObject
	{
	public:
		StabilityEstimator(unsigned int sampleSize, unsigned int minStableOccurrences,
			double threshold, double rateSimilarityLevel);
		virtual ~StabilityEstimator(){}

		void newDataArrived(double currentRate);
		bool isStable() const { return isStable_; }
        void flush();
        
        Average getDarrAverage() const { return m1_; }

	protected:
		unsigned int sampleSize_, minOccurrences_;
		double threshold_, rateSimilarityLevel_;
		Average m1_, m2_;
		unsigned int nStableOccurrences_, nUnstableOccurrences_;
		bool isStable_;
		uint64_t lastTimestamp_;
	};

	class DrdChangeEstimator : public ndnlog::new_api::ILoggingObject
	{
	public:
		DrdChangeEstimator(unsigned int sampleSize, unsigned int minStableOccurrences,
			double threshold);
		~DrdChangeEstimator(){}

		void newDrdValue(const Average& drdEstimator);
		bool hasChange();
		void flush();

	private:
		unsigned int sampleSize_, minOccurrences_;
		double threshold_;
		unsigned int nStableOccurrences_;
		bool isStable_;

		unsigned int nChanges_;
		unsigned int nMinorConsecutiveChanges_;
		unsigned int lastCheckedChangeNumber_;
	};
}

//******************************************************************************
StabilityEstimator::StabilityEstimator(unsigned int sampleSize, unsigned int minStableOccurrences,
    	double threshold, double rateSimilarityLevel):
sampleSize_(sampleSize), minOccurrences_(minStableOccurrences),
threshold_(threshold), rateSimilarityLevel_(rateSimilarityLevel),
m1_(Average(boost::make_shared<SampleWindow>(sampleSize_))),
m2_(Average(boost::make_shared<SampleWindow>(sampleSize_)))
{
	flush();
	description_ = "stability-estimator";
}

void
StabilityEstimator::newDataArrived(double currentRate)
{
	int64_t now = clock::millisecondTimestamp();

    if (lastTimestamp_ != 0)
    {
        unsigned int delta = now - lastTimestamp_;

        m2_.newValue(m1_.oldestValue());
        m1_.newValue(delta);

        double mean = m1_.value();
        double mean2 = m2_.value();
        
        if (mean != 0 && mean2 != 0)
        {
            double ratio = (mean2/mean);
            double targetDelay = 1000./currentRate;
            double similarityLevel = 1 - fabs(mean-targetDelay)/targetDelay;
            
            if (fabs(ratio-1) <= threshold_&&
                similarityLevel >= rateSimilarityLevel_)
            {
                nUnstableOccurrences_ = 0;
                nStableOccurrences_++;
                
                if (!isStable_)
                    LogTraceC << "stable occurrence #" << nStableOccurrences_ << std::endl;
            }
            else if (++nUnstableOccurrences_ >= minOccurrences_)
                nStableOccurrences_ = 0;
            
            isStable_ = (nStableOccurrences_ >= minOccurrences_);
            
            LogTraceC
            << "delta\t" << delta
            << "\tmean\t" << mean
            << "\tmean2\t" << mean2
            << "\tratio\t" << ratio
            << "\trate\t" << currentRate
            << "\ttarget delay\t" << targetDelay
            << "\tsim level\t" << similarityLevel
            << "\tstable\t" << (isStable_?"YES":"NO")
            << std::endl;
        }
        else
            LogTraceC
            << "delta\t" << delta
            << "\tmean\t" << mean
            << "\tmean2\t" << mean2
            << "\tstable\t" << (isStable_?"YES":"NO")
            << std::endl;
    }
    
    lastTimestamp_ = now;
}

void
StabilityEstimator::flush()
{
	isStable_ = false;
	nStableOccurrences_ = 0;
	nUnstableOccurrences_ = 0;
	lastTimestamp_ = 0;
	m1_ = Average(boost::make_shared<SampleWindow>(sampleSize_));
	m2_ = Average(boost::make_shared<SampleWindow>(sampleSize_));
}

//******************************************************************************
DrdChangeEstimator::DrdChangeEstimator(unsigned int sampleSize, 
	unsigned int minStableOccurrences, double threshold)
{
    description_ = "drd-change-est";
}

void
DrdChangeEstimator::newDrdValue(const Average& drdEstimator)
{
	double mean = drdEstimator.value();

	if (mean != 0)
	{
        double deviationPercentage = drdEstimator.deviation()/mean;
        
        if (deviationPercentage <= threshold_)
            nStableOccurrences_++;
        else
        {
            isStable_ = false;
            nChanges_ = 0;
            nMinorConsecutiveChanges_ = 0;
            nStableOccurrences_ = 0;
        }
        
        isStable_ = (nStableOccurrences_ >= minOccurrences_);
        
        if (isStable_)
        {
            double changePercentage = fabs(drdEstimator.latestValue()-mean)/mean;
            if (changePercentage >= 0.08)
            {
                if (changePercentage <= 0.2)
                {
                    nMinorConsecutiveChanges_++;
                    if (nMinorConsecutiveChanges_ >= sampleSize_)
                    {
                        nMinorConsecutiveChanges_ = 0;
                        nChanges_++;
                    }
                }
                else
                    nChanges_++;
            }
            else
                nMinorConsecutiveChanges_ = 0;
        }
        
        LogTraceC
        << "rtt\t" << drdEstimator.latestValue()
        << "\tmean\t" << mean
        << "\tdeviation\t" << deviationPercentage
        << "\tn changes\t" << nChanges_
        << "\tn minor\t" << nMinorConsecutiveChanges_
        << "\tstable\t" << (isStable_?"YES":"NO")
        << std::endl;

	}
}

bool
DrdChangeEstimator::hasChange()
{
    bool command = false;
    
    if (nChanges_ > lastCheckedChangeNumber_)
        command = true;
    
    lastCheckedChangeNumber_ = nChanges_;
    return command;
}

void
DrdChangeEstimator::flush()
{
    isStable_ = false;
    nStableOccurrences_ = 0;
    nChanges_ = 0;
    nMinorConsecutiveChanges_ = 0;
    lastCheckedChangeNumber_ = 0;
}

//******************************************************************************
LatencyControl::LatencyControl(unsigned int timeoutWindowMs, 
    const boost::shared_ptr<const DrdEstimator>& drd,
    const boost::shared_ptr<statistics::StatisticsStorage>& storage):
stabilityEstimator_(boost::make_shared<StabilityEstimator>(10, 4, 0.3, 0.7)),
//stabilityEstimator_(30, 4, 0.1, 0.95), // high
//stabilityEstimator_(3, 4, 0.6, 0.5), // low
drdChangeEstimator_(boost::make_shared<DrdChangeEstimator>(7, 3, 0.12)),
timestamp_(0),
waitForChange_(false), waitForStability_(false),
timeoutWindowMs_(timeoutWindowMs),
drd_(drd),
sstorage_(storage),
interArrival_(Average(boost::make_shared<estimators::SampleWindow>(10))),
targetRate_(30.),
observer_(nullptr),
currentCommand_(KeepPipeline)
{
    description_ = "latency-control";
}

LatencyControl::~LatencyControl()
{
}

void 
LatencyControl::onDrdUpdate()
{
    drdChangeEstimator_->newDrdValue(drd_->getLatestUpdatedAverage());
}

void
LatencyControl::sampleArrived(const PacketNumber& playbackNo)
{
    LogTraceC << "sample " << playbackNo << ". target rate " << targetRate_ << std::endl;

    PipelineAdjust command = KeepPipeline;
    int64_t now = clock::millisecondTimestamp();

    if (timestamp_ == 0) timestamp_ = now;

    bool timeoutFired = (now-timestamp_ > timeoutWindowMs_);
    stabilityEstimator_->newDataArrived(targetRate_);
    (*sstorage_)[Indicator::Darr] = stabilityEstimator_->getDarrAverage().latestValue();

    if (stabilityEstimator_->isStable())
    {
        if (waitForChange_)
        {
            if (drdChangeEstimator_->hasChange())
            {
                LogDebugC << "latest data. DRD changed ("
                    << drd_->getLatestUpdatedAverage().value() << "ms)"
                    << " wait for stabilization" << std::endl;

                timestamp_ = now;
                waitForStability_ = true;
                waitForChange_ = false;
            }
            else 
                if (timeoutFired)
                {
                    LogDebugC << "latest data. DRD change timed out. cmd: decrease" << std::endl;

                    timestamp_ = now;
                    command = DecreasePipeline;
                }
        }
        else 
        {
            LogDebugC << "latest data. cmd: decrease" << std::endl;

            timestamp_ = now;
            waitForStability_ = false;
            waitForChange_ = true;
            command = DecreasePipeline;
        }
    }
    else // if unstable and we're not waiting for anything - increase
        if (timeoutFired)
        {
            LogDebugC << "stale data. cmd: increase" << std::endl;

            timestamp_ = now;
            waitForStability_ = true;
            waitForChange_ = false;
            command = IncreasePipeline;
        }


    {
        boost::lock_guard<boost::mutex> scopedLock(mutex_);
        if (observer_ && observer_->needPipelineAdjustment(command))
            pipelineChanged();
    }

    currentCommand_ = command;
}

void 
LatencyControl::reset()
{
    stabilityEstimator_->flush();
    drdChangeEstimator_->flush();
    timestamp_ = 0;
    waitForChange_ = false;
    waitForStability_ = false;
    interArrival_ = Average(boost::make_shared<estimators::SampleWindow>(10));
    targetRate_ = 30.;
    currentCommand_ = KeepPipeline;
}

void 
LatencyControl::registerObserver(ILatencyControlObserver* o)
{
    boost::lock_guard<boost::mutex> scopedLock(mutex_);
    observer_ = o;
}

void 
LatencyControl::unregisterObserver()
{
    boost::lock_guard<boost::mutex> scopedLock(mutex_);
    observer_ = nullptr;
}

void
LatencyControl::setLogger(ndnlog::new_api::Logger* logger)
{
    NdnRtcComponent::setLogger(logger);
    stabilityEstimator_->setLogger(logger);
    drdChangeEstimator_->setLogger(logger);
}

#pragma mark - private
void 
LatencyControl::pipelineChanged()
{
    drdChangeEstimator_->flush();
}
