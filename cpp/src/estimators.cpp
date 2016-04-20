// 
// estimators.cpp
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <cstdlib>
#include <vector>
#include <cmath>

#include "estimators.h"
#include "clock.h"

using namespace ndnrtc::estimators;
using namespace ndnrtc::clock;
using namespace std;

typedef struct _FrequencyMeter {
    double cycleDuration_;
    unsigned int nCyclesPerSec_;
    double callsPerSecond_;
    int64_t lastCheckTime_;
    unsigned int avgEstimatorId_;
} FrequencyMeter;

typedef struct _DataRateMeter {
    double cycleDuration_;
    unsigned int bytesPerCycle_;
    double nBytesPerSec_;
    int64_t lastCheckTime_;
    unsigned int avgEstimatorId_;
} DataRateMeter;

typedef struct _MeanEstimator {
    unsigned int sampleSize_;
    int nValues_;
    double startValue_;
    double prevValue_;
    double valueMean_;
    double valueMeanSq_;
    double valueVariance_;
    double currentMean_;
    double currentDeviation_;
} MeanEstimator;

typedef struct _Filter {
    double coeff_;
    double filteredValue_;
} Filter;

typedef struct _InclineEstimator {
    unsigned int avgEstimatorId_;
    unsigned int nValues_;
    double lastValue_;
    unsigned int sampleSize_;
    unsigned int skipCounter_;
} InclineEstimator;

typedef struct _SlidingAverage {
    unsigned int sampleSize_;
    int nValues_;
    double accumulatedSum_;
    double currentAverage_;
    double currentDeviation_;
    double* sample_;
} SlidingAverage;

static vector<FrequencyMeter> freqMeters_;
static vector<DataRateMeter> dataMeters_;
static vector<MeanEstimator> meanEstimators_;
static vector<Filter> filters_;
static vector<InclineEstimator> inclineEstimators_;
static vector<SlidingAverage> slidingAverageEstimators_;

//******************************************************************************
//******************************************************************************
namespace ndnrtc {
	namespace estimators {
		unsigned int setupFrequencyMeter(unsigned int granularity)
		{
			FrequencyMeter meter = {1000./(double)granularity, 0, 0., 0, 0};
			meter.avgEstimatorId_ = setupSlidingAverageEstimator(granularity);

			freqMeters_.push_back(meter);

			return freqMeters_.size()-1;
		}

		void frequencyMeterTick(unsigned int meterId)
		{
			if (meterId >= freqMeters_.size())
				return;

			FrequencyMeter &meter = freqMeters_[meterId];
			int64_t now = millisecondTimestamp();
			int64_t delta = now - meter.lastCheckTime_;

			meter.nCyclesPerSec_++;

			if (delta >= meter.cycleDuration_)
				if (meter.lastCheckTime_ >= 0)
				{
					if (meter.lastCheckTime_ > 0)
					{
						meter.callsPerSecond_ = 1000.*(double)meter.nCyclesPerSec_/(double)(delta);
						meter.nCyclesPerSec_ = 0;
						slidingAverageEstimatorNewValue(meter.avgEstimatorId_, meter.callsPerSecond_);
					}

					meter.lastCheckTime_ = now;
				}
		}

		double currentFrequencyMeterValue(unsigned int meterId)
		{
				if (meterId >= freqMeters_.size())
					return 0.;

				FrequencyMeter &meter = freqMeters_[meterId];

				return currentSlidingAverageValue(meter.avgEstimatorId_);
		}

		void releaseFrequencyMeter(unsigned int meterId)
		{
				if (meterId >= freqMeters_.size())
					return;

    // do nothing
		}

//******************************************************************************
		unsigned int setupDataRateMeter(unsigned int granularity)
		{
				DataRateMeter meter = {1000./(double)granularity, 0, 0., 0, 0};
				meter.avgEstimatorId_ = setupSlidingAverageEstimator(10);

				dataMeters_.push_back(meter);

				return dataMeters_.size()-1;
		}

		void dataRateMeterMoreData(unsigned int meterId,
			unsigned int dataSize)
		{
				if (meterId >= dataMeters_.size())
					return ;

				DataRateMeter &meter = dataMeters_[meterId];
				int64_t now = millisecondTimestamp();
				int64_t delta = now - meter.lastCheckTime_;

				meter.bytesPerCycle_ += dataSize;

				if (delta >= meter.cycleDuration_)
					if (meter.lastCheckTime_ >= 0)
					{
						if (meter.lastCheckTime_ > 0)
						{
							meter.nBytesPerSec_ = 1000.*meter.bytesPerCycle_/delta;
							meter.bytesPerCycle_ = 0;
							slidingAverageEstimatorNewValue(meter.avgEstimatorId_, meter.nBytesPerSec_);
						}

						meter.lastCheckTime_ = now;
					}
			}

		double currentDataRateMeterValue(unsigned int meterId)
		{
					if (meterId >= dataMeters_.size())
						return 0.;

					DataRateMeter &meter = dataMeters_[meterId];
					return currentSlidingAverageValue(meter.avgEstimatorId_);
		}

		void releaseDataRateMeter(unsigned int meterId)
		{
					if (meterId >- dataMeters_.size())
						return;

    // nothing here yet
		}

//******************************************************************************
		unsigned int setupMeanEstimator(unsigned int sampleSize,
			double startValue)
		{
					MeanEstimator meanEstimator = {sampleSize, 0, startValue, startValue, startValue, 0., 0., startValue, 0.};

					meanEstimators_.push_back(meanEstimator);

					return meanEstimators_.size()-1;
		}

		void meanEstimatorNewValue(unsigned int estimatorId, double value)
		{
					if (estimatorId >= meanEstimators_.size())
						return ;

					MeanEstimator &estimator = meanEstimators_[estimatorId];

					estimator.nValues_++;

					if (estimator.nValues_ > 1)
					{
						double delta = (value - estimator.valueMean_);
						estimator.valueMean_ += (delta/(double)estimator.nValues_);
						estimator.valueMeanSq_ += (delta*delta);

						if (estimator.sampleSize_ == 0 ||
							estimator.nValues_ % estimator.sampleSize_ == 0)
						{
							estimator.currentMean_ = estimator.valueMean_;

							if (estimator.nValues_ >= 2)
							{
								double variance = estimator.valueMeanSq_/(double)(estimator.nValues_-1);
								estimator.currentDeviation_ = sqrt(variance);
							}

            // flush nValues if sample size specified
							if (estimator.sampleSize_)
								estimator.nValues_ = 0;
						}
					}
					else
					{
						estimator.currentMean_ = value;
						estimator.valueMean_ = value;
						estimator.valueMeanSq_ = 0;
						estimator.valueVariance_ = 0;
					}

					estimator.prevValue_ = value;
		}

		double currentMeanEstimation(unsigned int estimatorId)
		{
					if (estimatorId >= meanEstimators_.size())
						return 0;

					MeanEstimator& estimator = meanEstimators_[estimatorId];

					return estimator.currentMean_;
		}

		double currentDeviationEstimation(unsigned int estimatorId)
		{
					if (estimatorId >= meanEstimators_.size())
						return 0;

					MeanEstimator& estimator = meanEstimators_[estimatorId];

					return estimator.currentDeviation_;
		}

		void releaseMeanEstimator(unsigned int estimatorId)
		{
					if (estimatorId >= meanEstimators_.size())
						return ;

    // nothing
		}

		void resetMeanEstimator(unsigned int estimatorId)
		{
					if (estimatorId >= meanEstimators_.size())
						return ;

					MeanEstimator& estimator = meanEstimators_[estimatorId];

					estimator = (MeanEstimator){estimator.sampleSize_, 0, estimator.startValue_,
						estimator.startValue_, estimator.startValue_, 0., 0.,
						estimator.startValue_, 0.};
		}

//******************************************************************************
		unsigned int
		setupInclineEstimator(unsigned int sampleSize)
		{
						InclineEstimator ie;
						ie.sampleSize_ = sampleSize;
						ie.nValues_ = 0;
						ie.avgEstimatorId_ = setupSlidingAverageEstimator(sampleSize);
						ie.lastValue_ = 0.;
						ie.skipCounter_ = 0;
						inclineEstimators_.push_back(ie);

						return inclineEstimators_.size()-1;
		}

		void
		inclineEstimatorNewValue(unsigned int estimatorId, double value)
		{
						if (estimatorId < inclineEstimators_.size())
						{
							InclineEstimator& ie = inclineEstimators_[estimatorId];

							if (ie.nValues_ == 0)
								ie.lastValue_ = value;
							else
							{
								double incline = (value-ie.lastValue_);

								slidingAverageEstimatorNewValue(ie.avgEstimatorId_, incline);
								ie.lastValue_ = value;
							}

							ie.nValues_++;
						}
		}

		double
		currentIncline(unsigned int estimatorId)
		{
						if (estimatorId < inclineEstimators_.size())
						{
							InclineEstimator& ie = inclineEstimators_[estimatorId];
							return currentSlidingAverageValue(ie.avgEstimatorId_);
						}

						return 0.;
		}

		void releaseInclineEstimator(unsigned int estimatorId)
		{
    // tbd
		}

//******************************************************************************
		unsigned int setupSlidingAverageEstimator(unsigned int sampleSize)
		{
						SlidingAverage slidingAverage;

						slidingAverage.sampleSize_ = (sampleSize == 0)?2:sampleSize;
						slidingAverage.nValues_ = 0;
						slidingAverage.accumulatedSum_ = 0.;
						slidingAverage.currentAverage_ = 0.;
						slidingAverage.currentDeviation_ = 0.;

						slidingAverage.sample_ = (double*)malloc(sizeof(double)*sampleSize);
						memset(slidingAverage.sample_, 0, sizeof(double)*sampleSize);

						slidingAverageEstimators_.push_back(slidingAverage);

						return slidingAverageEstimators_.size()-1;
		}

		double slidingAverageEstimatorNewValue(unsigned int estimatorId, double value)
		{
						if (estimatorId >= slidingAverageEstimators_.size())
							return 0.;

						double oldValue = 0;
						SlidingAverage& estimator = slidingAverageEstimators_[estimatorId];

						oldValue = estimator.sample_[estimator.nValues_%estimator.sampleSize_];
						estimator.sample_[estimator.nValues_%estimator.sampleSize_] = value;
						estimator.nValues_++;

						if (estimator.nValues_ >= estimator.sampleSize_)
						{
							estimator.currentAverage_ = (estimator.accumulatedSum_+value)/estimator.sampleSize_;

							estimator.accumulatedSum_ += value - estimator.sample_[estimator.nValues_%estimator.sampleSize_];

							estimator.currentDeviation_ = 0.;
							for (int i = 0; i < estimator.sampleSize_; i++)
								estimator.currentDeviation_ += (estimator.sample_[i]-estimator.currentAverage_)*(estimator.sample_[i]-estimator.currentAverage_);
							estimator.currentDeviation_ = sqrt(estimator.currentDeviation_/(double)estimator.nValues_);
						}
						else
							estimator.accumulatedSum_ += value;

						return oldValue;
		}

		double currentSlidingAverageValue(unsigned int estimatorId)
		{
						if (estimatorId >= slidingAverageEstimators_.size())
							return 0;

						SlidingAverage& estimator = slidingAverageEstimators_[estimatorId];
						return estimator.currentAverage_;
		}

		double currentSlidingDeviationValue(unsigned int estimatorId)
		{
						if (estimatorId >= slidingAverageEstimators_.size())
							return 0;

						SlidingAverage& estimator = slidingAverageEstimators_[estimatorId];
						return estimator.currentDeviation_;
		}

		void resetSlidingAverageEstimator(unsigned int estimatorID)
		{
						if (estimatorID >= slidingAverageEstimators_.size())
							return ;

						SlidingAverage& estimator = slidingAverageEstimators_[estimatorID];
						estimator.nValues_ = 0;
						estimator.accumulatedSum_ = 0.;
						estimator.currentAverage_ = 0.;
						estimator.currentDeviation_ = 0.;
						memset(estimator.sample_, 0, sizeof(double)*estimator.sampleSize_);
		}

		void releaseAverageEstimator(unsigned int estimatorID)
		{
    // nothing
		}

//******************************************************************************
		unsigned int
		setupFilter(double coeff)
		{
						Filter filter = {coeff, 0.};
						filters_.push_back(filter);
						return filters_.size()-1;
		}

		void
		filterNewValue(unsigned int filterId, double value)
		{
						if (filterId < filters_.size())
						{
							Filter &filter = filters_[filterId];

							if (filter.filteredValue_ == 0)
								filter.filteredValue_ = value;
							else
								filter.filteredValue_ += (value-filter.filteredValue_)*filter.coeff_;
						}
		}

		double
		currentFilteredValue(unsigned int filterId)
		{
						if (filterId < filters_.size())
							return filters_[filterId].filteredValue_;

						return 0.;
		}

		void
		releaseFilter(unsigned int filterId)
		{
    // do nothing
		}
	}
}

#if 0
//******************************************************************************
Rate::Rate(unsigned int updateFrequency):Estimator(),
cycleDurationMs_(1000./(double)updateFrequency),
nCyclesPerSec_(0),
callsPerSecond_(0),
lastCheckTime_(0)
{

}

void Rate::tick()
{

}

#endif