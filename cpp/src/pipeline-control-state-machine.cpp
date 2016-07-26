// 
// pipeline-control-state-machine.cpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "pipeline-control-state-machine.h"
#include <boost/make_shared.hpp>
#include <boost/assign.hpp>

#include "clock.h"
#include "latency-control.h"
#include "interest-control.h"
#include "pipeliner.h"
#include "frame-data.h"
#include "playout-control.h"

using namespace ndnrtc;
namespace ndnrtc {
	const std::string kStateIdle = "Idle";
	const std::string kStateWaitForRightmost = "WaitForRightmost";
	const std::string kStateWaitForInitial = "WaitForInitial";
	const std::string kStateChasing = "Chasing";
	const std::string kStateAdjusting = "Adjusting";
	const std::string kStateFetching = "Fetching";
}

#define STATE_TRANSITION(s,t)(std::pair<std::string, PipelineControlEvent::Type>(s,PipelineControlEvent::Type::t))
#define MAKE_TRANSITION(s,t)(std::pair<std::string, PipelineControlEvent::Type>(s,t))

namespace ndnrtc {
	class PipelineControlState {
	public:
		PipelineControlState(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):ctrl_(ctrl){}

		virtual std::string str() const = 0;

		/**
		 * Called when state is entered
		 */
		virtual void enter(){}

		/**
		 * Called when state is exited
		 */
		virtual void exit(){}
		
		/**
		 * Called when upon new event
		 * @param event State machine event
		 * @return Next state transition to
		 */
		virtual std::string dispatchEvent(const boost::shared_ptr<const PipelineControlEvent>& ev);

		bool operator==(const PipelineControlState& other) const
		{ return str() == other.str(); }

	protected:
		boost::shared_ptr<PipelineControlStateMachine::Struct> ctrl_;

		virtual std::string onStart(const boost::shared_ptr<const PipelineControlEvent>&)
		{ return str(); }
		virtual std::string onReset(const boost::shared_ptr<const PipelineControlEvent>& ev)
		{ return str(); }
		virtual std::string onStarvation(const boost::shared_ptr<const EventStarvation>& ev)
		{ return str(); }
		virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout>& ev)
		{ return str(); }
		virtual std::string onSegment(const boost::shared_ptr<const EventSegment>& ev)
		{ return str(); }
	};

	/**
	 * Idle state. System is in idle state when it first created.
	 * On entry:
	 * 	- resets control structures (pipeliner, interest control, latency control, etc.)
	 * On exit:
	 * 	- nothing
	 * Processed events: 
	 * 	- Start: switches to WaitForRightmost
	 * 	- Reset: resets control structures
	 */
	class Idle : public PipelineControlState {
	public:
		Idle(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):PipelineControlState(ctrl){}

		std::string str() const { return kStateIdle; }

		virtual void enter();
	};

	/**
	 * WaitForRightmost state. Sytem is in this state while waiting for the answer of 
	 * the rightmost Interest.
	 * On entry:
	 * 	- sends out rightmost Interest (accesses pipeliner)
	 * On exit:
	 * 	- nothing
	 * Processed events: 
	 *	- Start: ignored
	 *	- Reset: resets to idle
	 *	- Starvation: ignored
	 *	- Timeout: re-issue Interest (accesses pipeliner)
	 *	- Segment: transition to WaitForInitial state
	 */
	class WaitForRightmost : public PipelineControlState {
	public:
		WaitForRightmost(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):PipelineControlState(ctrl){}

		std::string str() const { return kStateWaitForRightmost; }
		virtual void enter();

	protected:
		virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout>& ev);
		virtual std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);

		virtual void askRightmost();
		virtual void receivedRightmost(const boost::shared_ptr<const EventSegment>& es);
	};

	/**
	 * WaitForRightmostKey state is a subclass of WaitForRightmost which requests for
	 * rightmost data in Key namespace, instead of default (Delta).
	 */
	class WaitForRightmostKey : public WaitForRightmost {
	public:
		WaitForRightmostKey(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):WaitForRightmost(ctrl){}

	private:
		void askRightmost();
	};

	/**
	 * WaitForInitial state. System is in this state while waiting for initial data
	 * to arrive. Initial data - first data requested with specific sequence number 
	 * learned from previously received rightmost data.
	 * On entry:
	 * 	- sends out Interest batch for a specific sequence number learned from 
	 *		previously received righmost data (accesses pipeliner)
	 * On exit:
	 * 	- nothing
	 * Processed events: 
	 *	- Start: ignored
	 *	- Reset: resets to idle
	 *	- Starvation: ignores
	 *	- Timeout: re-issue Interest 3 times and then resets to idle (accesses pipeliner)
	 *	- Segment: transition to Chasing state
	 */
	class WaitForInitial : public PipelineControlState {
	public:
		WaitForInitial(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):PipelineControlState(ctrl){}

		std::string str() const { return kStateWaitForInitial; }
		void enter();

	protected:
		unsigned int nTimeouts_;

		virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout>& ev);
		virtual std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);
	};

	/**
	 * WaitForInitialKey state is a subclass of WaitForInitial which requests
	 * key frames instead of default (delta) samples as initial data. It 
	 * initializes pipeliner sequence number based on received segment metada.
	 */
	class WaitForInitialKey : public WaitForInitial
	{
	public:
		WaitForInitialKey(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):WaitForInitial(ctrl){}
	private:
		std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);
	};

	/**
	 * Chasing state. System is in this state while is tries to catch up with the 
	 * latest data from the network.
	 * On entry:
	 * 	- does nothing
	 * On exit:
	 * 	- nothing
	 * Processed events: 
	 *	- Start: ignored
	 *	- Reset: resets to idle
	 *	- Starvation: resets to idle
	 *	- Timeout: re-issue Interest (accesses pipeliner)
	 *	- Segment: checks interest control (for pipeline increases), checks latency 
	 * 		control whether latest  data arrival has been detected, if so, 
	 *		transitions to Adjusting
	 */
	class Chasing : public PipelineControlState {
	public:
		Chasing(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):PipelineControlState(ctrl){}

		std::string str() const { return kStateChasing; }
	private:
		virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout>& ev);
		virtual std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);
	};

	/**
	 * Adjusting state. System is in this state while it tries to minimize the size
	 * of the pipeline.
	 * On entry:
	 * 	- does nothing
	 * On exit:
	 * 	- nothing
	 * Processed events: 
	 *	- Start: ignored
	 *	- Reset: resets to idle
	 *	- Starvation: resets to idle
	 *	- Timeout: re-issue Interest (accesses pipeliner)
	 *	- Segment: checks interest control (for pipeline decreases), checks latency 
	 *		control whether latest data arrival stopped, if so, backs up to previous
	 *		pipeline size and transitions to Fetching
	 */
	class Adjusting : public PipelineControlState {
	public:
		Adjusting(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):PipelineControlState(ctrl){}

		std::string str() const { return kStateAdjusting; }
		void enter();
	private:
		unsigned int pipelineLowerLimit_;

		std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);
	};

	/**
	 * Fetching state. System is in this state when it receives latest data and 
	 * the pipeline size is minimized.
	 * On entry:
	 * 	- does nothing
	 * On exit:
	 * 	- nothing
	 * Processed events: 
	 *	- Start: ignored
	 *	- Reset: resets to idle
	 *	- Starvation: resets to idle
	 *	- Timeout: re-issue Interest (accesses pipeliner)
	 *	- Segment: checks interest control, checks latency control, transitions to
	 * 		Adjust state if latest data arrival stops
	 */
	class Fetching : public PipelineControlState {
	public:
		Fetching(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl):PipelineControlState(ctrl){}

		std::string str() const { return kStateFetching; }
	private:
		std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);
	};
}

//******************************************************************************
std::string
PipelineControlEvent::toString() const
{
	switch (e_)
	{
		case PipelineControlEvent::Start: return "Start";
		case PipelineControlEvent::Reset: return "Reset";
		case PipelineControlEvent::Starvation: return "Starvation";
		case PipelineControlEvent::Segment: return "Segment";
		case PipelineControlEvent::Timeout: return "Timeout";
		default: return "Unknown";
	}
}

//******************************************************************************
PipelineControlStateMachine 
PipelineControlStateMachine::defaultStateMachine(PipelineControlStateMachine::Struct ctrl)
{
	boost::shared_ptr<PipelineControlStateMachine::Struct> 
		pctrl(boost::make_shared<PipelineControlStateMachine::Struct>(ctrl));
	return PipelineControlStateMachine(pctrl, defaultConsumerStatesMap(pctrl)); 
}

PipelineControlStateMachine 
PipelineControlStateMachine::videoStateMachine(Struct ctrl)
{
	boost::shared_ptr<PipelineControlStateMachine::Struct> 
		pctrl(boost::make_shared<PipelineControlStateMachine::Struct>(ctrl));
	return PipelineControlStateMachine(pctrl, videoConsumerStatesMap(pctrl)); 
}

PipelineControlStateMachine::StatesMap
PipelineControlStateMachine::defaultConsumerStatesMap(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl)
{
	boost::shared_ptr<PipelineControlState> idle(boost::make_shared<Idle>(ctrl));
	PipelineControlStateMachine::StatesMap map = boost::assign::map_list_of 
		(kStateIdle, idle)
		(kStateWaitForRightmost, boost::make_shared<WaitForRightmost>(ctrl))
		(kStateWaitForInitial, boost::make_shared<WaitForInitial>(ctrl))
		(kStateChasing, boost::make_shared<Chasing>(ctrl))
		(kStateAdjusting, boost::make_shared<Adjusting>(ctrl))
		(kStateFetching, boost::make_shared<Fetching>(ctrl));
	return map;
}

PipelineControlStateMachine::StatesMap
PipelineControlStateMachine::videoConsumerStatesMap(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl)
{
	boost::shared_ptr<PipelineControlState> idle(boost::make_shared<Idle>(ctrl));
	PipelineControlStateMachine::StatesMap map = boost::assign::map_list_of 
		(kStateIdle, idle)
		(kStateWaitForRightmost, boost::make_shared<WaitForRightmostKey>(ctrl))
		(kStateWaitForInitial, boost::make_shared<WaitForInitialKey>(ctrl))
		(kStateChasing, boost::make_shared<Chasing>(ctrl))
		(kStateAdjusting, boost::make_shared<Adjusting>(ctrl))
		(kStateFetching, boost::make_shared<Fetching>(ctrl));
	return map;
}

//******************************************************************************
PipelineControlStateMachine::PipelineControlStateMachine(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl,
	PipelineControlStateMachine::StatesMap statesMap):
ppCtrl_(ctrl),
states_(statesMap),
currentState_(states_[kStateIdle]),
lastEventTimestamp_(clock::millisecondTimestamp())
{
    assert(ppCtrl_->buffer_.get());
    assert(ppCtrl_->pipeliner_.get());
	assert(ppCtrl_->interestControl_.get());
	assert(ppCtrl_->latencyControl_.get());
    assert(ppCtrl_->playoutControl_.get());

	currentState_->enter();
	description_ = "state-machine";

	stateMachineTable_ = boost::assign::map_list_of
		(STATE_TRANSITION(kStateIdle, Start), 					kStateWaitForRightmost)
		(STATE_TRANSITION(kStateWaitForRightmost, Reset), 		kStateIdle)
		(STATE_TRANSITION(kStateWaitForInitial, Reset), 		kStateIdle)
		(STATE_TRANSITION(kStateChasing, Reset), 				kStateIdle)
		(STATE_TRANSITION(kStateChasing, Starvation), 			kStateIdle)
		(STATE_TRANSITION(kStateAdjusting, Reset), 				kStateIdle)
		(STATE_TRANSITION(kStateAdjusting, Starvation), 		kStateIdle)
		(STATE_TRANSITION(kStateFetching, Reset), 				kStateIdle)
		(STATE_TRANSITION(kStateFetching, Starvation), 			kStateIdle);
}

PipelineControlStateMachine::~PipelineControlStateMachine()
{
	currentState_->exit();
}

std::string
PipelineControlStateMachine::getState() const
{ return currentState_->str(); }

void
PipelineControlStateMachine::dispatch(const boost::shared_ptr<const PipelineControlEvent>& ev)
{
	std::string nextState = currentState_->dispatchEvent(ev);

	// if we got new state - transition to it
	if (nextState != currentState_->str())
	{
		if (states_.find(nextState) == states_.end())
			throw std::runtime_error(std::string("Unsupported state: "+nextState).c_str());
		switchToState(nextState, ev->toString());
	}
	else 
		transition(ev);
}

#pragma mark - private

bool
PipelineControlStateMachine::transition(const boost::shared_ptr<const PipelineControlEvent>& ev)
{
	if (stateMachineTable_.find(MAKE_TRANSITION(currentState_->str(), ev->getType())) ==
		stateMachineTable_.end())
		return false;

	switchToState(stateMachineTable_[MAKE_TRANSITION(currentState_->str(), ev->getType())],
                  ev->toString());
	return true;
}

void
PipelineControlStateMachine::switchToState(const std::string& state,
                                           const std::string& event)
{
    int64_t now = clock::millisecondTimestamp();
    int64_t stateDuration = (lastEventTimestamp_ ? now - lastEventTimestamp_ : 0);
    lastEventTimestamp_ = now;
    
    LogInfoC << "[" << currentState_->str() << "]-("
        << event << ")->[" << states_[state]->str() << "] "
        << stateDuration << "ms" << std::endl;

	currentState_->exit();
	currentState_ = states_[state];
	currentState_->enter();
}

//******************************************************************************
std::string 
PipelineControlState::dispatchEvent(const boost::shared_ptr<const PipelineControlEvent>& ev)
{
	switch (ev->getType())
	{
		case PipelineControlEvent::Start: return onStart(ev);
		case PipelineControlEvent::Reset: return onReset(ev);
		case PipelineControlEvent::Starvation: 
			return onStarvation(boost::dynamic_pointer_cast<const EventStarvation>(ev));
		case PipelineControlEvent::Timeout:
			return onTimeout(boost::dynamic_pointer_cast<const EventTimeout>(ev));
		case PipelineControlEvent::Segment: 
			return onSegment(boost::dynamic_pointer_cast<const EventSegment>(ev));
		default: return str();
	}
}

//******************************************************************************
void 
Idle::enter()
{
    ctrl_->buffer_->reset();
	ctrl_->pipeliner_->reset();
	ctrl_->latencyControl_->reset();
	ctrl_->interestControl_->reset();
    ctrl_->playoutControl_->allowPlayout(false);
}

//******************************************************************************
void
WaitForRightmost::enter()
{
	askRightmost();
}

std::string
WaitForRightmost::onSegment(const boost::shared_ptr<const EventSegment>& ev)
{
	receivedRightmost(boost::dynamic_pointer_cast<const EventSegment>(ev));
	return kStateWaitForInitial;
}

std::string
WaitForRightmost::onTimeout(const boost::shared_ptr<const EventTimeout>& ev)
{
	askRightmost();
	return str();
}

void 
WaitForRightmost::askRightmost()
{
	ctrl_->pipeliner_->setNeedRightmost();
	ctrl_->pipeliner_->express(ctrl_->threadPrefix_);
}

void
WaitForRightmost::receivedRightmost(const boost::shared_ptr<const EventSegment>& ev)
{
	ctrl_->pipeliner_->setSequenceNumber(ev->getSegment()->getSampleNo()+1,
		ev->getSegment()->getSampleClass());
	ctrl_->pipeliner_->setNeedSample(ev->getSegment()->getSampleClass());
	ctrl_->pipeliner_->express(ctrl_->threadPrefix_, true);
    ctrl_->interestControl_->increment();
}

//******************************************************************************
void 
WaitForRightmostKey::askRightmost()
{
	ctrl_->pipeliner_->setNeedSample(SampleClass::Key);
	return WaitForRightmost::askRightmost();
}

//******************************************************************************
void
WaitForInitial::enter()
{
	nTimeouts_ = 0;
}

std::string
WaitForInitial::onTimeout(const boost::shared_ptr<const EventTimeout>& ev)
{
	if (++nTimeouts_ > 3)
		return kStateIdle;

	ctrl_->pipeliner_->setNeedSample(ev->getInfo().class_);
	ctrl_->pipeliner_->express(ctrl_->threadPrefix_);
	return str();
}

std::string
WaitForInitial::onSegment(const boost::shared_ptr<const EventSegment>& ev)
{
	nTimeouts_ = 0;
	ctrl_->pipeliner_->segmentArrived(ctrl_->threadPrefix_);
	return kStateChasing;
}

//******************************************************************************
std::string
WaitForInitialKey::onSegment(const boost::shared_ptr<const EventSegment>& ev)
{
	boost::shared_ptr<const WireData<VideoFrameSegmentHeader>> seg = 
		boost::dynamic_pointer_cast<const WireData<VideoFrameSegmentHeader>>(ev->getSegment());

	ctrl_->pipeliner_->setSequenceNumber(seg->segment().getHeader().pairedSequenceNo_,
		SampleClass::Delta);
    ctrl_->pipeliner_->setSequenceNumber(seg->getSampleNo()+1,
                                         SampleClass::Key);

	return WaitForInitial::onSegment(ev);
}

//******************************************************************************
std::string 
Chasing::onTimeout(const boost::shared_ptr<const EventTimeout>& ev)
{
	ctrl_->pipeliner_->express(ev->getInfo().getPrefix(prefix_filter::Thread));
	return str();
}

std::string 
Chasing::onSegment(const boost::shared_ptr<const EventSegment>& ev)
{
	ctrl_->pipeliner_->segmentArrived(ctrl_->threadPrefix_);

	if (ctrl_->latencyControl_->getCurrentCommand() == PipelineAdjust::DecreasePipeline)
		return kStateAdjusting;
	return str();
}

//******************************************************************************
void 
Adjusting::enter()
{
	pipelineLowerLimit_ = ctrl_->interestControl_->pipelineLimit();
	ctrl_->playoutControl_->allowPlayout(true);
}

std::string 
Adjusting::onSegment(const boost::shared_ptr<const EventSegment>& ev)
{
	ctrl_->pipeliner_->segmentArrived(ctrl_->threadPrefix_);
    
	PipelineAdjust cmd = ctrl_->latencyControl_->getCurrentCommand();
	
	if (cmd == PipelineAdjust::IncreasePipeline)
	{
		ctrl_->interestControl_->markLowerLimit(pipelineLowerLimit_);
		return kStateFetching;
	}

	if (cmd == PipelineAdjust::DecreasePipeline)
		pipelineLowerLimit_ = ctrl_->interestControl_->pipelineLimit();

	return str();
}

//******************************************************************************
std::string
Fetching::onSegment(const boost::shared_ptr<const EventSegment>& ev)
{
	ctrl_->pipeliner_->segmentArrived(ctrl_->threadPrefix_);

	if (ctrl_->latencyControl_->getCurrentCommand() == PipelineAdjust::IncreasePipeline)
		return kStateAdjusting;
	return str();
}
