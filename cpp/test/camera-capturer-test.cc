//
//  camera-capturer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//

//#define DEBUG
#define NDN_LOGGING
#define NDN_DETAILED

#define NDN_TRACE
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_DEBUG

#include "gtest/gtest.h"
#include "simple-log.h"
#include "camera-capturer.h"

using namespace ndnrtc;

//********************************************************************************
/**
 * @name CameraCaptureParams class tests
 */
TEST(CameraCapturerParamsTest, CreateDelete) {
    NdnParams *p = NdnParams::defaultParams();
    delete p;
//    CameraCapturerParams *params = CameraCapturerParams::defaultParams();
//    delete params;
}
