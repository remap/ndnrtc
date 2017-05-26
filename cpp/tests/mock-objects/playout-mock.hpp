// 
// playout-mock.hpp
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __playout_mock_h__
#define __playout_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "playout.hpp"

class MockPlayout : public ndnrtc::IPlayout {
public:
	MOCK_METHOD1(start, void(unsigned int));
	MOCK_METHOD0(stop, void());
	MOCK_CONST_METHOD0(isRunning, bool());
};

#endif