// 
// external-capturer-mock.h
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __external_capturer_mock_h__
#define __external_capturer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <include/interfaces.h>

class MockExternalCapturer : public ndnrtc::IExternalCapturer
{
public:
	MOCK_METHOD0(capturingStarted, void(void));
	MOCK_METHOD0(capturingStopped, void(void));
	MOCK_METHOD4(incomingArgbFrame, int(const unsigned int width,
                                      const unsigned int height,
                                      unsigned char* argbFrameData,
                                      unsigned int frameSize));
	MOCK_METHOD8(incomingI420Frame, int(const unsigned int width,
                                      const unsigned int height,
                                      const unsigned int strideY,
                                      const unsigned int strideU,
                                      const unsigned int strideV,
                                      const unsigned char* yBuffer,
                                      const unsigned char* uBuffer,
                                      const unsigned char* vBuffer));
};

#endif
