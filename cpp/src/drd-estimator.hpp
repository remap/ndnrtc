//
// drd-estimator.hpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __drd_estimator_h__
#define __drd_estimator_h__

#include <vector>
#include <boost/thread/mutex.hpp>

#include "estimators.hpp"

namespace ndnrtc
{
class IDrdEstimatorObserver;

/**
 * DRD Estimator class runs estimations for DRD (Data Retrieval Delay) values 
 * using sliding average estimators. 
 * Estimator runs two estimations - one for original data (answered by previously 
 * issued Interest) and one for data coming from cache.
 * @see SlotSegment::isOriginal()
 */
class DrdEstimator
{
  public:
    DrdEstimator(unsigned int initialEstimationMs = 150, unsigned int windowMs = 200);

    void setInitialEstimation(double initial) { initialEstimation_ = initial; }
    void newValue(double drd, bool isOriginal, double dGen);
    double getCachedEstimation() const;
    double getOriginalEstimation() const;
    void reset();

    const estimators::Average &getCachedAverage() const { return cachedDrd_; }
    const estimators::Average &getOriginalAverage() const { return originalDrd_; }
    const estimators::Average &getLatestUpdatedAverage() const { return *latest_; }
    const estimators::Average &getGenerationDelayAverage() const { return generationDelay_; }

    void attach(IDrdEstimatorObserver *o);
    void detach(IDrdEstimatorObserver *o);

  private:
    boost::mutex mutex_;
    std::vector<IDrdEstimatorObserver *> observers_;
    unsigned int windowSize_, initialEstimation_;
    estimators::Average cachedDrd_, originalDrd_;
    estimators::Average generationDelay_;
    estimators::Average *latest_;
};

class IDrdEstimatorObserver
{
  public:
    virtual void onDrdUpdate() = 0;
    virtual void onCachedDrdUpdate(double, double) = 0;
    virtual void onOriginalDrdUpdate(double, double) = 0;
};
}

#endif