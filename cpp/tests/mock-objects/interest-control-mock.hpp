// 
// interest-control-mock.hpp
//
//  Created by Peter Gusev on 13 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __interest_control_mock_h__
#define __interest_control_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "interest-control.hpp"

class MockInterestControl : public ndnrtc::IInterestControl
{
public:
	MOCK_METHOD2(initialize, void(double, int));
    MOCK_METHOD0(reset, void(void));
	MOCK_METHOD0(decrement, bool(void));
	MOCK_METHOD0(increment, bool(void));
	MOCK_CONST_METHOD0(pipelineLimit, size_t(void));
	MOCK_CONST_METHOD0(pipelineSize, size_t(void));
	MOCK_CONST_METHOD0(room, int(void));
	MOCK_METHOD0(burst, bool());
	MOCK_METHOD0(withhold, bool());
	MOCK_METHOD1(markLowerLimit, void(unsigned int));
    MOCK_CONST_METHOD0(snapshot, std::string());
};

#endif