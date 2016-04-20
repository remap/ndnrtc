// 
// estimators.h
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __estimators_h__
#define __estimators_h__

#include <stdlib.h>

namespace ndnrtc {
	namespace estimators {

		unsigned int setupFrequencyMeter(unsigned int granularity = 1);
        void frequencyMeterTick(unsigned int meterId);
        double currentFrequencyMeterValue(unsigned int meterId);
        void releaseFrequencyMeter(unsigned int meterId);

        unsigned int setupDataRateMeter(unsigned int granularity = 1);
        void dataRateMeterMoreData(unsigned int meterId,
                                          unsigned int dataSize);
        double currentDataRateMeterValue(unsigned int meterId);
        void releaseDataRateMeter(unsigned int meterId);
        
        unsigned int setupMeanEstimator(unsigned int sampleSize = 0,
                                               double startValue = 0.);
        void meanEstimatorNewValue(unsigned int estimatorId, double value);
        double currentMeanEstimation(unsigned int estimatorId);
        double currentDeviationEstimation(unsigned int estimatorId);
        void releaseMeanEstimator(unsigned int estimatorId);
        void resetMeanEstimator(unsigned int estimatorId);
        
        unsigned int setupSlidingAverageEstimator(unsigned int sampleSize = 2);
        double slidingAverageEstimatorNewValue(unsigned int estimatorId, double value);
        double currentSlidingAverageValue(unsigned int estimatorId);
        double currentSlidingDeviationValue(unsigned int estimatorId);
        void resetSlidingAverageEstimator(unsigned int estimatorID);
        void releaseAverageEstimator(unsigned int estimatorID);
        
        unsigned int setupFilter(double coeff = 1.);
        void filterNewValue(unsigned int filterId, double value);
        double currentFilteredValue(unsigned int filterId);
        void releaseFilter(unsigned int filterId);
        
        unsigned int setupInclineEstimator(unsigned int sampleSize = 0);
        void inclineEstimatorNewValue(unsigned int estimatorId, double value);
        double currentIncline(unsigned int estimatorId);
        void releaseInclineEstimator(unsigned int estimatorId);

		// TBD: refactor estimators into classes
#if 0
		/**
		 * Base class for estimators
		 */
		class Estimator {
		public:
			Estimator():value_(0){}
			virtual double value() { return value_; }

		protected:
			double value_;
		};

		/**
		 * Sliding window estimator calculates average and deviation over time 
		 * window
		 */
		class SlidingWindow : public Estimator {
		public:
			SlidingWindow(){}
		private:

		};

		/**
		 * This class implements rate estimator for measuring rates of
		 * arbitrary events (like frame encoding) per second.
		 */
		class Rate : public Estimator {
		public:
			/**
			 * Create an instance of Rate class. 
			 * @param updateFrequency How often (times per sec) estimator will 
			 * update its' value
			 */
			Rate(unsigned int updateFrequency);

			/**
			 * This method should be called each time measured event happens
			 */
			void tick();

		private:
			double cycleDurationMs_;
			unsigned int nCyclesPerSec_;
			double callsPerSecond_;
			int64_t lastCheckTime_;
			unsigned int avgEstimatorId_;
		};
#endif
	}
}

#endif