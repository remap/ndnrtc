// 
// pipeline-control.cpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "pipeline-control.h"
#include <boost/assign.hpp>

#include "interest-control.h"
#include "pipeline-control-state-machine.h"

using namespace ndnrtc;

//******************************************************************************
PipelineControl PipelineControl::defaultPipelineControl(const ndn::Name& threadPrefix,
			const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl)
{
	PipelineControlStateMachine::Struct ctrl(threadPrefix);
	ctrl.pipeliner_ = pipeliner;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
	return PipelineControl(PipelineControlStateMachine::defaultStateMachine(ctrl),
		interestControl);
}

PipelineControl PipelineControl::videoPipelineControl(const ndn::Name& threadPrefix,
			const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl)
{
	PipelineControlStateMachine::Struct ctrl(threadPrefix);
	ctrl.pipeliner_ = pipeliner;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
	return PipelineControl(PipelineControlStateMachine::videoStateMachine(ctrl),
		interestControl);
}

//******************************************************************************
PipelineControl::PipelineControl(const PipelineControlStateMachine& machine,
			const boost::shared_ptr<IInterestControl>& interestControl):
machine_(machine),
interestControl_(interestControl)
{
	description_ = "pipeline-control";
}

void 
PipelineControl::start()
{
	if (machine_.getState() != kStateIdle)
		throw std::runtime_error("Can't start Pipeline Control as it has been "
			"started alread. Use reset() and start() to restart.");

	machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));

	LogDebugC << "started" << std::endl;
}

void 
PipelineControl::stop()
{
	machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Reset));
	LogDebugC << "stopped" << std::endl;
}

void 
PipelineControl::segmentArrived(const boost::shared_ptr<WireSegment>& s)
{
	machine_.dispatch(boost::make_shared<EventSegment>(s));
}

void 
PipelineControl::segmentRequestTimeout(const NamespaceInfo& n)
{
	machine_.dispatch(boost::make_shared<EventTimeout>(n));
}

void 
PipelineControl::segmentStarvation()
{
	machine_.dispatch(boost::make_shared<EventStarvation>(500));
}

bool 
PipelineControl::needPipelineAdjustment(const PipelineAdjust& cmd)
{
	if (cmd == PipelineAdjust::IncreasePipeline) interestControl_->burst();
	if (cmd == PipelineAdjust::DecreasePipeline) interestControl_->withhold();
}

#pragma mark - private
