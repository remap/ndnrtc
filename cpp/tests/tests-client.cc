// 
// test-client.cc
//
//  Created by Peter Gusev on 09 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include "gtest/gtest.h"
#include "client/src/client.hpp"

TEST(TestClient, TestSingleton)
{
	Client& c = Client::getSharedInstance();
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
