//
//  test-main.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//

#include <stdio.h>
#include <dlfcn.h>

#include "gtest/gtest.h"

GTEST_API_ int main(int argc, char **argv)
{
	  testing::InitGoogleTest(&argc, argv);
	  return RUN_ALL_TESTS();
}