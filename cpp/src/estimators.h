// 
// estimators.h
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __estimators_h__
#define __estimators_h__

#include <stdlib.h>
#include <assert.h>
#include <deque>
#include <boost/shared_ptr.hpp>
#include <boost/move/move.hpp>

#include "ndnrtc-defines.h"

namespace ndnrtc {
	namespace estimators {
		/**
		 * Interface for estimator window class. 
		 * An estimator window defines an interval in some dimension, over 
		 * which estimator is operating. For instance, if 
		 * estimator window is in time dimension and has value of 5 seconds, 
		 * then estimator is operating over window of 5 seconds. Likewise,
		 * if window's dimension is the number of samples, then estimator is 
		 * operating over the window of 5 samples.
		 */
		class IEstimatorWindow {
		public:
			/**
			 * Indicates progress over the window.
			 * Must be called every time estimator receives a new value.
			 * @return true if window limit has been reached, false otherwise
			 */
			virtual bool isLimitReached() = 0;
		};

		class SampleWindow : public IEstimatorWindow {
		public:
			SampleWindow(unsigned int nSamples):nSamples_(nSamples),remaining_(nSamples)
			{ assert(nSamples_); }

			bool isLimitReached();
		private:
			unsigned int nSamples_, remaining_;
		};

		class TimeWindow : public IEstimatorWindow {
		public:
			TimeWindow(unsigned int milliseconds);

			bool isLimitReached();
		private:
			unsigned int milliseconds_;
			int64_t lastReach_;
		};

		/**
		 * Base class for estimators
		 */
		class Estimator {
		public:
			Estimator(boost::shared_ptr<IEstimatorWindow> window):
				value_(0),window_(window),nValues_(0){}
			
			virtual void newValue(double value) = 0;
			virtual double value() const { return value_; }
			unsigned int count() const { return nValues_; }

		protected:
			unsigned int nValues_;
			double value_;
			boost::shared_ptr<IEstimatorWindow> window_;
		};


		/**
		 * Sliding window estimator calculates average and deviation over time 
		 * window
		 */
		class Average : public Estimator {
		public:
			Average(boost::shared_ptr<IEstimatorWindow> window);

			void newValue(double value);
			double deviation() const { return sqrt(variance_); }
			double variance() const { return variance_; }
			double oldestValue() const { return samples_.front(); }
			double latestValue() const { return samples_.back(); }

		private:
			bool limitReached_;
			std::deque<double> samples_;
			double accumulatedSum_, variance_;
		};

		/**
		 * Frequency estimator measures average frequency (per second) of new value 
		 * appearings. Meter value is updated every window interval.
		 */
		class FreqMeter : public Estimator {
		public:
			FreqMeter(boost::shared_ptr<IEstimatorWindow> window);

			/**
			 * Passed value is ignored. This call is used to calculate frequency of 
			 * calling this method over the estimator window.
			 */
			void newValue(double value);

		private:
			unsigned int nCalls_;
			int64_t ts_;
		};

		/**
		 * A low pass filter class
		 */
		class Filter {
		public:
			Filter(double smoothing = 1./8.);

			double value() const { return value_; }
			void newValue(double value);
		
		private:
			double smoothing_,value_;
		};
	}
}

#endif