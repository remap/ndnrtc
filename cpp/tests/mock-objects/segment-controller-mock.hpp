// 
// segment-controller-mock.hpp
//
//  Created by Peter Gusev on 13 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __segment_controller_mock_h__
#define __segment_controller_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "segment-controller.hpp"

class MockSegmentController : public ndnrtc::ISegmentController
{
public:
	MOCK_CONST_METHOD0(getCurrentIdleTime, unsigned int());
	MOCK_CONST_METHOD0(getMaxIdleTime, unsigned int());
	MOCK_METHOD0(getOnDataCallback, ndn::OnData());
	MOCK_METHOD0(getOnTimeoutCallback, ndn::OnTimeout());
	MOCK_METHOD0(getOnNetworkNackCallback, ndn::OnNetworkNack());
	MOCK_METHOD1(attach, void(ndnrtc::ISegmentControllerObserver*));
	MOCK_METHOD1(detach, void(ndnrtc::ISegmentControllerObserver*));
};

#endif