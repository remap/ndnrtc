// 
// latency-control.h
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __latency_control_h__
#define __latency_control_h__

#include <boost/thread/mutex.hpp>

#include "drd-estimator.h"
#include "segment-controller.h"
#include "ndnrtc-object.h"
#include "buffer-control.h"

namespace ndnrtc {
	class StabilityEstimator;
	class DrdChangeEstimator;
	class ILatencyControlObserver;

	/**
	 * Latency control runs estimation of latest data arrival and detects
	 * changes in DRD when pipeline size changes. In order for it to work 
	 * properly, one should call frameArrived() each time first segment of
	 * new sample has arrived (regardless of segment number) and 
	 * setTargetRate() whenever new information about target sample rate 
	 * has became available or updated.
	 */
	class LatencyControl :  public IDrdEstimatorObserver,
							public NdnRtcComponent,
							public IBufferControlObserver
	{
	public:
		typedef enum _Command {
			IncreasePipeline,
			DecreasePipeline,
			KeepPipeline
		} Command;

		LatencyControl(unsigned int timeoutWindowMs, 
			const boost::shared_ptr<const DrdEstimator>& drd);
		~LatencyControl();

		void onDrdUpdate();
		void onCachedDrdUpdate(){ /*ignored*/ }
		void onOriginalDrdUpdate(){ /*ignored*/ }

		void targetRateUpdate(double rate) { targetRate_ = rate; }
		void sampleArrived(const PacketNumber& playbackNo);
		void reset();

		void registerObserver(ILatencyControlObserver* o);
		void unregisterObserver();

		Command getCurrentCommand() const { return currentCommand_; }

	private:
		boost::mutex mutex_;
		boost::shared_ptr<StabilityEstimator> stabilityEstimator_;
		boost::shared_ptr<DrdChangeEstimator> drdChangeEstimator_;
		bool waitForChange_, waitForStability_;
		int64_t timestamp_;
		unsigned int timeoutWindowMs_;
		boost::shared_ptr<const DrdEstimator> drd_;
		estimators::Average interArrival_;
		double targetRate_;
		ILatencyControlObserver* observer_;
		Command currentCommand_;

		void pipelineChanged();
	};

	class ILatencyControlObserver {
	public:
		virtual bool needPipelineAdjustment(const LatencyControl::Command&) = 0;
	};
}

#endif
