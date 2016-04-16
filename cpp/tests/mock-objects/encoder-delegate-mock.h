// 
// encoder-delegate-mock.h
//
//  Created by Peter Gusev on 15 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __encoder_delegate_mock_h__
#define __encoder_delegate_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/video-coder.h"

class MockEncoderDelegate : public ndnrtc::new_api::IEncoderDelegate
{
public:
	MOCK_METHOD0(onEncodingStarted, void(void));
	MOCK_METHOD1(onEncodedFrame, void(const webrtc::EncodedImage&));
	MOCK_METHOD0(onDroppedFrame, void(void));

	void setDefaults() {
		ON_CALL(*this, onEncodingStarted())
			.WillByDefault(testing::Invoke(this, &MockEncoderDelegate::countEncodingStarted));
		ON_CALL(*this, onEncodedFrame(testing::_))
			.WillByDefault(testing::Invoke(this, &MockEncoderDelegate::countEncodedFrame));
		ON_CALL(*this, onDroppedFrame())
			.WillByDefault(testing::Invoke(this, &MockEncoderDelegate::countDroppedFrame));
	}

	int getEncodedNum() { return encComplete_; }
	int getDroppedNum() { return dropped_; }
	int getBytesReceived() { return receivedBytes_; }
	int getDelta() { return nDelta_; }
	int getKey() {return nKey_; }
private:
	int encStarted_ = 0, encComplete_ = 0, dropped_ = 0, receivedBytes_;
	int nDelta_ = 0, nKey_ = 0;

	void countEncodingStarted() { encStarted_++; }
	void countDroppedFrame() { dropped_++; }
	void countEncodedFrame(const webrtc::EncodedImage& f) 
	{ 
		if (f._frameType == webrtc::kKeyFrame) nKey_++;
		else nDelta_++;
		encComplete_++; receivedBytes_ += f._length; 
	}
};

#endif