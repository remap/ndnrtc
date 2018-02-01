// 
// pipeline-control-state-machine-observer-mock.hpp
//
//  Created by Peter Gusev on 14 September 2017.
//  Copyright 2013-2017 Regents of the University of California
//

#ifndef __pipeline_control_state_machine_observer_mock_hpp__
#define __pipeline_control_state_machine_observer_mock_hpp__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "pipeline-control-state-machine.hpp"

class MockPipelineControlStateMachineObserver : public ndnrtc::IPipelineControlStateMachineObserver {
public:
	MOCK_METHOD2(onStateMachineChangedState, void(const boost::shared_ptr<const ndnrtc::PipelineControlEvent>&,
			std::string newState));
	MOCK_METHOD2(onStateMachineReceivedEvent, void(const boost::shared_ptr<const ndnrtc::PipelineControlEvent>&,
			std::string state));
};

#endif