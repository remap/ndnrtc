// 
// pipeline-control-state-machine.cpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "pipeline-control-state-machine.hpp"
#include <boost/make_shared.hpp>
#include <boost/assign.hpp>

#include "clock.hpp"
#include "latency-control.hpp"
#include "interest-control.hpp"
#include "pipeliner.hpp"
#include "frame-data.hpp"
#include "playout-control.hpp"
#include "statistics.hpp"

using namespace ndnrtc;
using namespace ndnrtc::statistics;

namespace ndnrtc {
	const std::string kStateIdle = "Idle";
	const std::string kStateWaitForRightmost = "WaitForRightmost";
	const std::string kStateWaitForInitial = "WaitForInitial";
	const std::string kStateChasing = "Chasing";
	const std::string kStateAdjusting = "Adjusting";
	const std::string kStateFetching = "Fetching";
}

#define STATE_TRANSITION(s,t)(StateEventPair(s,PipelineControlEvent::Type::t))
#define MAKE_TRANSITION(s,t)(StateEventPair(s,t))

namespace ndnrtc {
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
        int toInt() { return (int)StateId::Idle; }
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
        int toInt() { return (int)StateId::WaitForRightmost; }
        
	protected:
		virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout>& ev);
		virtual std::string onNack(const boost::shared_ptr<const EventNack>& ev);
		virtual std::string onSegment(const boost::shared_ptr<const EventSegment>& ev);
		virtual std::string onStarvation(const boost::shared_ptr<const EventStarvation>& ev);

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
        int toInt() { return (int)StateId::WaitForInitial; }
        
	protected:
		unsigned int nTimeouts_;

		virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout>& ev);
		virtual std::string onNack(const boost::shared_ptr<const EventNack>& ev);
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
        int toInt() { return (int)StateId::Chasing; }
        
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
        int toInt() { return (int)StateId::Adjusting; }
        
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
        int toInt() { return (int)StateId::Fetching; }
        
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

	// add indirection to avoid confusion in C++11 (Ubuntu)
	const TransitionMap m = boost::assign::map_list_of
		(STATE_TRANSITION(kStateIdle, Start), 					kStateWaitForRightmost)
		(STATE_TRANSITION(kStateWaitForRightmost, Reset), 		kStateIdle)
		(STATE_TRANSITION(kStateWaitForInitial, Reset), 		kStateIdle)
		(STATE_TRANSITION(kStateChasing, Reset), 				kStateIdle)
		(STATE_TRANSITION(kStateChasing, Starvation), 			kStateIdle)
		(STATE_TRANSITION(kStateAdjusting, Reset), 				kStateIdle)
		(STATE_TRANSITION(kStateAdjusting, Starvation), 		kStateIdle)
		(STATE_TRANSITION(kStateFetching, Reset), 				kStateIdle)
		(STATE_TRANSITION(kStateFetching, Starvation), 			kStateIdle);
	stateMachineTable_ = m;
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
    // dispatchEvent allows current state to react to the event.
    // if state need to be switched, then next state name is returned.
    // every state knows its own behavior to the event.
    // state might also ignore the event. in this case, it returns
    // its own name.
	std::string nextState = currentState_->dispatchEvent(ev);

	// if we got new state - transition to it
	if (nextState != currentState_->str())
	{
		if (states_.find(nextState) == states_.end())
			throw std::runtime_error(std::string("Unsupported state: "+nextState).c_str());
		switchToState(states_[nextState], ev);
	}
	else
        // otherwise - check whether state machine table defines transition
        // for this event
		if (!transition(ev))
		{
			for (auto o:observers_)
				o->onStateMachineReceivedEvent(ev, currentState_->str());
		}
}

void
PipelineControlStateMachine::attach(IPipelineControlStateMachineObserver *observer)
{
	if (observer)
		observers_.push_back(observer);
}

void
PipelineControlStateMachine::detach(IPipelineControlStateMachineObserver *observer)
{
	std::vector<IPipelineControlStateMachineObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), observer);
	if (it != observers_.end())
		observers_.erase(it);
}


#pragma mark - private
bool
PipelineControlStateMachine::transition(const boost::shared_ptr<const PipelineControlEvent>& ev)
{
	if (stateMachineTable_.find(MAKE_TRANSITION(currentState_->str(), ev->getType())) ==
		stateMachineTable_.end())
		return false;

    std::string stateStr = stateMachineTable_[MAKE_TRANSITION(currentState_->str(), ev->getType())];
	switchToState(states_[stateStr], ev);

	return true;
}

void
PipelineControlStateMachine::switchToState(const boost::shared_ptr<PipelineControlState>& state,
                   const boost::shared_ptr<const PipelineControlEvent>& event)
{
    int64_t now = clock::millisecondTimestamp();
    int64_t stateDuration = (lastEventTimestamp_ ? now - lastEventTimestamp_ : 0);
    lastEventTimestamp_ = now;
    
    LogInfoC << "[" << currentState_->str() << "]-("
    << event->toString() << ")->[" << state->str() << "] "
    << stateDuration << "ms" << std::endl;
    
    currentState_->exit();
    currentState_ = state;
    currentState_->enter();

    for (auto o:observers_)
    	o->onStateMachineChangedState(event, currentState_->str());
    
    if (event->toString() == boost::make_shared<EventStarvation>(0)->toString())
        (*ppCtrl_->sstorage_)[Indicator::RebufferingsNum]++;
    (*ppCtrl_->sstorage_)[Indicator::State] = (double)state->toInt();
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

std::string 
WaitForRightmost::onNack(const boost::shared_ptr<const EventNack>& ev)
{
	//askRightmost(); // really?
	return str();
}

std::string
WaitForRightmost::onStarvation(const boost::shared_ptr<const EventStarvation>& ev)
{
	// askRightmost(); // maybe?
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
WaitForInitial::onNack(const boost::shared_ptr<const EventNack>& ev)
{
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
	ctrl_->pipeliner_->express(ev->getInfo().getPrefix(prefix_filter::ThreadNT));
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
	{
		// ctrl_->interestControl_->markLowerLimit(interestControl::MinPipelineSize);
		return kStateAdjusting;
	}

	return str();
}
