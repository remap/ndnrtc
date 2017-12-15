// 
// rtx-observer-mock.hpp
//
//  Created by Peter Gusev on 23 August 2017.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __rtx_observer_mock_h__
#define __rtx_observer_mock_h__

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "rtx-controller.hpp"

class MockRtxObserver : public ndnrtc::IRtxObserver {
public:
	MOCK_METHOD1(onRetransmissionRequired, void(const std::vector<boost::shared_ptr<const ndn::Interest>>&));
};

#endif