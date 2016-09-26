// 
// pipeline-control.cpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "pipeline-control.hpp"
#include <boost/assign.hpp>

#include "interest-control.hpp"
#include "pipeline-control-state-machine.hpp"
#include "frame-data.hpp"

using namespace ndnrtc;

//******************************************************************************
PipelineControl PipelineControl::defaultPipelineControl(const ndn::Name& threadPrefix,
            const boost::shared_ptr<IBuffer> buffer,
			const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl,
			const boost::shared_ptr<IPlayoutControl> playoutControl,
            const boost::shared_ptr<statistics::StatisticsStorage>& storage)
{
	PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pipeliner;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
	ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
	return PipelineControl(PipelineControlStateMachine::defaultStateMachine(ctrl),
		interestControl);
}

PipelineControl PipelineControl::videoPipelineControl(const ndn::Name& threadPrefix,
			const boost::shared_ptr<IBuffer> buffer,
            const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl,
			const boost::shared_ptr<IPlayoutControl> playoutControl,
            const boost::shared_ptr<statistics::StatisticsStorage>& storage)
{
	PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.buffer_ = buffer;
	ctrl.pipeliner_ = pipeliner;
	ctrl.interestControl_ = interestControl;
	ctrl.latencyControl_ = latencyControl;
	ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
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
    if (s->getSampleClass() == SampleClass::Key ||
        s->getSampleClass() == SampleClass::Delta)
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
	if (cmd == PipelineAdjust::IncreasePipeline ||
		cmd == PipelineAdjust::DecreasePipeline)
	{
		if (cmd == PipelineAdjust::IncreasePipeline) interestControl_->burst();
		if (cmd == PipelineAdjust::DecreasePipeline) interestControl_->withhold();
		return true;
	}
	return false;
}

void
PipelineControl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    NdnRtcComponent::setLogger(logger);
    machine_.setLogger(logger);
}

#pragma mark - private
