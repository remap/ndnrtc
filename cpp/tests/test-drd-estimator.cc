// 
// test-drd-estimator.cc
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "src/drd-estimator.h"

#include "mock-objects/drd-estimator-observer-mock.h"

using namespace ::testing;
using namespace ndnrtc;

TEST(TestDrdEstimator, TestDefault)
{
	MockDrdEstimatorObserver o;
	DrdEstimator drd;
	drd.attach(&o);
	EXPECT_EQ(150, drd.getOriginalEstimation());
	EXPECT_EQ(150, drd.getCachedEstimation());

	EXPECT_CALL(o, onOriginalDrdUpdate())
		.Times(100);
	EXPECT_CALL(o, onCachedDrdUpdate())
		.Times(100);
	EXPECT_CALL(o, onDrdUpdate())
		.Times(200);

	for (int i = 1; i <= 100; ++i)
		drd.newValue(i, true);

	for (double i = 1; i <= 50.5; i+=0.5)
		drd.newValue(i, false);

	EXPECT_EQ(50.5, drd.getOriginalEstimation());
	EXPECT_EQ(25.75, drd.getCachedEstimation());
	EXPECT_EQ(25.75, drd.getLatestUpdatedAverage().value());

	drd.reset();
	EXPECT_EQ(150, drd.getOriginalEstimation());
	EXPECT_EQ(150, drd.getCachedEstimation());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
