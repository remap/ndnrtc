// 
// external-capturer-mock.hpp
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __external_capturer_mock_h__
#define __external_capturer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <include/interfaces.hpp>

class MockExternalCapturer : public ndnrtc::IExternalCapturer
{
public:
	MOCK_METHOD4(incomingArgbFrame, int(const unsigned int width,
                                      const unsigned int height,
                                      unsigned char* argbFrameData,
                                      unsigned int frameSize,
                                      unsigned int userDataSize,
                                      unsigned char* userData));
	MOCK_METHOD8(incomingI420Frame, int(const unsigned int width,
                                      const unsigned int height,
                                      const unsigned int strideY,
                                      const unsigned int strideU,
                                      const unsigned int strideV,
                                      const unsigned char* yBuffer,
                                      const unsigned char* uBuffer,
                                      const unsigned char* vBuffer,
                                      unsigned int userDataSize,
                                      unsigned char* userData));
};

#endif
