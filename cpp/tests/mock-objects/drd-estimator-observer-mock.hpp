// 
// drd-estimator-observer-mock.hpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//


#ifndef __drd_estimator_observer_mock_h__
#define __drd_estimator_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/drd-estimator.hpp"

class MockDrdEstimatorObserver : public ndnrtc::IDrdEstimatorObserver
{
public:
	MOCK_METHOD0(onDrdUpdate, void());
	MOCK_METHOD2(onCachedDrdUpdate, void(double, double));
	MOCK_METHOD2(onOriginalDrdUpdate, void(double, double));
};

#endif
