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

GTEST_API_ int main(int argc, char **argv) {
//
//  if (dlopen("libndnrtc.dylib",RTLD_LAZY) == NULL)
//		printf("error while loading NdnRTC library: %s \n", dlerror());
//	else
//	{
//		printf("successfully loaded NdnRTC library\n");	
//		  printf("running main() from test_main.cc\n");
	  testing::InitGoogleTest(&argc, argv);
	  return RUN_ALL_TESTS();
//	  }
//  return 1;
}