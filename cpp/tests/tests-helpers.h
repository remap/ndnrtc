// 
// tests-helpers.h
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __tests_helpers_h__
#define __tests_helpers_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <include/params.h>
#include <include/interfaces.h>
#include <include/statistics.h>

#include "client/src/config.h"

ndnrtc::VideoCoderParams sampleVideoCoderParams();
ClientParams sampleConsumerParams();
ClientParams sampleProducerParams();

namespace testing
{
 namespace internal
 {
  enum GTestColor {
      COLOR_DEFAULT,
      COLOR_RED,
      COLOR_GREEN,
      COLOR_YELLOW
  };

  extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
 }
}
#define GT_PRINTF(...)  do { testing::internal::ColoredPrintf(testing::internal::COLOR_GREEN, "[ INFO     ] "); testing::internal::ColoredPrintf(testing::internal::COLOR_YELLOW, __VA_ARGS__); } while(0)

#endif
