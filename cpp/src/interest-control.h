// 
// interest-control.h
//
//  Created by Peter Gusev on 09 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __interest_control_h__
#define __interest_control_h__

#include <deque>
#include <boost/atomic.hpp>

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "drd-estimator.h"
#include "buffer-control.h"

namespace ndn {
	class Name;
}

namespace ndnrtc {
	class WireSegment;

	class IInterestControl
	{
	public:
		virtual void reset() = 0;
		virtual bool decrement() = 0;
		virtual bool increment() = 0;
		virtual size_t pipelineLimit() const = 0;
		virtual size_t pipelineSize() const = 0;
		virtual int room() const = 0;
		virtual bool burst() = 0;
		virtual bool withhold() = 0;
		virtual void markLowerLimit(unsigned int lowerLimit) = 0;
	};

	/**
	 * InterestControl implements algorithm for lambda control or Intersts 
	 * expression control. It keeps track of the max limit size of the pipeline
	 * of outstanding Interests, current size of the pipeline and size of 
	 * the room for exressing mode Interests.
	 */
	class InterestControl : public NdnRtcComponent,
							public IInterestControl,
							public IDrdEstimatorObserver,
							public IBufferControlObserver
	{
	public:
		/**
		 * Interface for Interest pipeline adjustment strategy
		 */
		class IStrategy {
		public:
			virtual void getLimits(double rate, const estimators::Average& drd, 
				unsigned int& lowerLimit, unsigned int& upperLimit) = 0;
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
		class StrategyDefault : public IStrategy {
		public:
			void getLimits(double rate, const estimators::Average& drd, 
				unsigned int& lowerLimit, unsigned int& upperLimit);
			int burst(unsigned int currentLimit, 
				unsigned int lowerLimit, unsigned int upperLimit);
			int withhold(unsigned int currentLimit, 
				unsigned int lowerLimit, unsigned int upperLimit);
		};

		InterestControl(const boost::shared_ptr<DrdEstimator>&, 
			boost::shared_ptr<IStrategy> strategy = boost::make_shared<StrategyDefault>());
		~InterestControl();

		void reset();
		
		/**
		 * Should be called every time when pipeline needs to be decremented,
		 * for instance, when previously requested data arrives.
		 * @return true if pipeline was incremented, false if pipeline can't be 
		 * 	incremented
		 */
		bool decrement();
		
		/**
		 * Should be called every time when pipeline needs to be incremented,
		 * for instance, when Interests towards a new sample were issued.
		 * @return true if pipeline was decremented, false if pipeline can't be
		 * 	decremented (room is less than or equal zero)
		 * @see room
		 */
		bool increment();

		/**
		 * Returns current pipeline maximum size. This size can be changed 
		 * by client.
		 * @see decrease(), increase()
		 */
		size_t pipelineLimit() const { return limit_; }

		/**
		 * Returns current pipeline size, i.e. number of outstanding samples
		 */
		size_t pipelineSize() const { return pipeline_; }

		/**
		 * Returns size of the room for expressing more Interests. If room is 
		 * zero or negative, no Interests for new samples should be expressed.
		 */
		int room() const { return limit_-pipeline_; }

		/**
		 * Increases current pipeline limit, if possible.
		 * @return true if limit was increased, false otherwise
		 */
		bool burst();

		/**
		 * Decreases current pipeline limit, if possible.
		 * @return true if decrease was possible, false - otherwise
		 */
		bool withhold();

		/**
		 * Sets lower limit for pipeline size
		 */
		void markLowerLimit(unsigned int lowerLimit);

		void onDrdUpdate();
		void onCachedDrdUpdate(){ /*ignored*/ }
		void onOriginalDrdUpdate(){ /*ignored*/ }

		void targetRateUpdate(double rate);
		void sampleArrived(const PacketNumber&) { decrement(); }

	private:
		boost::shared_ptr<IStrategy> strategy_;
		boost::atomic<bool> initialized_, limitSet_;
		unsigned int lowerLimit_, limit_, upperLimit_;
		boost::atomic<int> pipeline_;
		boost::shared_ptr<DrdEstimator> drdEstimator_;
		double targetRate_;

		void setLimits();
		void changeLimitTo(unsigned int newLimit);
		std::string snapshot() const;
	};
}

#endif