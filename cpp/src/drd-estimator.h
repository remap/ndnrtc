// 
// drd-estimator.h
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __drd_estimator_h__
#define __drd_estimator_h__

#include <vector>
#include <boost/thread/mutex.hpp>

#include "estimators.h"

namespace ndnrtc {
	class IDrdEstimatorObserver;

	/**
	 * DRD Estimator class runs estimations for DRD (Data Retrieval Delay) values 
	 * using sliding average estimators. 
	 * Estimator runs two estimations - one for original data (answered by previously 
	 * issued Interest) and one for data coming from cache.
	 * @see SlotSegment::isOriginal()
	 */
	class DrdEstimator {
	public:
		DrdEstimator(unsigned int initialEstimationMs = 150, unsigned int windowMs = 30000);

		void newValue(double drd, bool isOriginal);
		double getCachedEstimation() const;
		double getOriginalEstimation() const;
		void reset();

		const estimators::Average& getCachedAverage() const { return cachedDrd_; }
		const estimators::Average& getOriginalAverage() const { return originalDrd_; }

		void attach(IDrdEstimatorObserver* o);
		void detach(IDrdEstimatorObserver* o);

	private:
		boost::mutex mutex_;
		std::vector<IDrdEstimatorObserver*> observers_;
		unsigned int windowSize_, initialEstimation_;
		estimators::Average cachedDrd_, originalDrd_;
	};

	class IDrdEstimatorObserver
	{
	public:
		virtual void onDrdUpdate() = 0;
		virtual void onCachedDrdUpdate() = 0;
		virtual void onOriginalDrdUpdate() = 0;
	};
}

#endif