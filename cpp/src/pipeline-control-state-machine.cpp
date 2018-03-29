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
	const std::string kStateBootstrapping = "Bootstrapping";
	const std::string kStateAdjusting = "Adjusting";
	const std::string kStateFetching = "Fetching";
}

#define STATE_TRANSITION(s,t)(StateEventPair(s,PipelineControlEvent::Type::t))
#define MAKE_TRANSITION(s,t)(StateEventPair(s,t))
#define ENABLE_IF(T, M) template <typename U = T, typename boost::enable_if<typename boost::is_same<M, U>>::type... X>

template <typename MetadataClass>
class ReceivedMetadataProcessing
{
  protected:
    ENABLE_IF(MetadataClass, VideoThreadMeta)
    bool processMetadata(boost::shared_ptr<VideoThreadMeta> metadata,
                         boost::shared_ptr<PipelineControlStateMachine::Struct> ctrl)
    {
        if (metadata)
        {
            unsigned char gopPos = metadata->getGopPos();
            unsigned int gopSize = metadata->getCoderParams().gop_;
            PacketNumber deltaToFetch, keyToFetch;
            unsigned int pipelineInitial = InterestControl::MinPipelineSize;

            // add some smart logic about what to fetch next...
            if (gopPos < ((float)gopSize / 2.))
            {
                // should probably fetch current key
                deltaToFetch = metadata->getSeqNo().first;
                keyToFetch = metadata->getSeqNo().second;
                pipelineInitial += gopPos;
            }
            else
            {
                // should fetch next key
                deltaToFetch = metadata->getSeqNo().first;
                keyToFetch = metadata->getSeqNo().second + 1;
                pipelineInitial += (gopSize-gopPos);
            }

            ctrl->interestControl_->initialize(metadata->getRate(), pipelineInitial);
            ctrl->pipeliner_->setSequenceNumber(deltaToFetch, SampleClass::Delta);
            ctrl->pipeliner_->setSequenceNumber(keyToFetch, SampleClass::Key);
            ctrl->pipeliner_->setNeedSample(SampleClass::Key);
            ctrl->pipeliner_->express(ctrl->threadPrefix_, true);
            ctrl->pipeliner_->setNeedSample(SampleClass::Delta);
            ctrl->pipeliner_->express(ctrl->threadPrefix_, true);

            return true;
        }

        return false;
    }

    ENABLE_IF(MetadataClass, AudioThreadMeta)
    bool processMetadata(boost::shared_ptr<AudioThreadMeta> metadata,
                         boost::shared_ptr<PipelineControlStateMachine::Struct> ctrl)
    {
        if (metadata)
        {
            PacketNumber bundleNo = metadata->getBundleNo();
            unsigned int pipelineInitial = 2 * InterestControl::MinPipelineSize;

            ctrl->interestControl_->initialize(metadata->getRate(), pipelineInitial);
            ctrl->pipeliner_->setSequenceNumber(bundleNo, SampleClass::Delta);
            ctrl->pipeliner_->setNeedSample(SampleClass::Delta);
            ctrl->pipeliner_->express(ctrl->threadPrefix_, true);

            return true;
        }

        return false;
    }

    boost::shared_ptr<MetadataClass> extractMetadata(boost::shared_ptr<NetworkData> data)
    {
        return boost::make_shared<MetadataClass>(boost::move(*data));
    }

    boost::shared_ptr<MetadataClass> extractMetadata(boost::shared_ptr<const WireSegment> segment)
    {
        ImmutableHeaderPacket<DataSegmentHeader> packet(segment->getData()->getContent());
        NetworkData nd(packet.getPayload().size(), packet.getPayload().data());
        return boost::make_shared<MetadataClass>(boost::move(nd));
    }
};

/**
 * Idle state. System is in idle state when it first created.
 * On entry:
 * 	- resets control structures (pipeliner, interest control, latency control, etc.)
 * On exit:
 * 	- nothing
 * Processed events: 
 * 	- Start: switches to Bootstrapping
 *  - Init: switches to Adjusting
 * 	- Reset: resets control structures
 */
template <typename MetadataClass>
class IdleT : public PipelineControlState,
              public ReceivedMetadataProcessing<MetadataClass>
{
  public:
    IdleT(const boost::shared_ptr<PipelineControlStateMachine::Struct> &ctrl) : PipelineControlState(ctrl) {}

    std::string str() const override { return kStateIdle; }
    void enter() override
    {
        ctrl_->buffer_->reset();
        ctrl_->pipeliner_->reset();
        ctrl_->latencyControl_->reset();
        ctrl_->interestControl_->reset();
        ctrl_->playoutControl_->allowPlayout(false);
    }
    int toInt() override { return (int)StateId::Idle; }

  protected:
    std::string onInit(const boost::shared_ptr<const EventInit> &ev) override
    {
        boost::shared_ptr<MetadataClass> metadata = 
            ReceivedMetadataProcessing<MetadataClass>::extractMetadata(ev->getNetworkData());

        if (metadata && ReceivedMetadataProcessing<MetadataClass>::processMetadata(metadata, ctrl_))
            return kStateAdjusting;

        return kStateBootstrapping;
    }
};

typedef IdleT<AudioThreadMeta> IdleAudio;
typedef IdleT<VideoThreadMeta> IdleVideo;

/**
 * Bootstrapping state. Sytem is in this state while waiting for the answer of 
 * the thread metadata Interest.
 * On entry:
 * 	- sends out metadata Interest (accesses pipeliner)
 * On exit:
 * 	- nothing
 * Processed events: 
 *	- Start: ignored
 *	- Reset: resets to idle
 *	- Starvation: ignored
 *	- Timeout: re-issue Interest (accesses pipeliner)
 *	- Segment: transition to Adjusting state
 */
template <typename MetadataClass>
class BootstrappingT : public PipelineControlState,
                      public ReceivedMetadataProcessing<MetadataClass>
{
  public:
    BootstrappingT(const boost::shared_ptr<PipelineControlStateMachine::Struct> &ctrl) : PipelineControlState(ctrl) {}

    std::string str() const override { return kStateBootstrapping; }
    void enter() override { askMetadata(); }
    int toInt() override { return (int)StateId::Bootstrapping; }

  protected:
    std::string onTimeout(const boost::shared_ptr<const EventTimeout> &ev) override
    {
        askMetadata();
        return str();
    }
    // std::string onNack(const boost::shared_ptr<const EventNack> &ev) override;
    std::string onSegment(const boost::shared_ptr<const EventSegment> &ev) override
    {
        return receivedMetadata(boost::dynamic_pointer_cast<const EventSegment>(ev));
    }

    // std::string onStarvation(const boost::shared_ptr<const EventStarvation> &ev) override;

    void askMetadata()
    {
        ctrl_->pipeliner_->setNeedMetadata();
        ctrl_->pipeliner_->express(ctrl_->threadPrefix_);
    }

    std::string receivedMetadata(const boost::shared_ptr<const EventSegment> &ev)
    {
        boost::shared_ptr<MetadataClass> metadata = 
            ReceivedMetadataProcessing<MetadataClass>::extractMetadata(ev->getSegment());

        if (metadata && ReceivedMetadataProcessing<MetadataClass>::processMetadata(metadata, ctrl_))
            return kStateAdjusting;

        return kStateBootstrapping;
    }
};

typedef BootstrappingT<AudioThreadMeta> BootstrappingAudio;
typedef BootstrappingT<VideoThreadMeta> BootstrappingVideo;

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
	 *		control whether latest data arrival stopped, if so, restores previous
	 *		pipeline size and transitions to Fetching state
	 */
class Adjusting : public PipelineControlState
{
  public:
    Adjusting(const boost::shared_ptr<PipelineControlStateMachine::Struct> &ctrl) : PipelineControlState(ctrl) {}

    std::string str() const override { return kStateAdjusting; }
    void enter() override;
    int toInt() override { return (int)StateId::Adjusting; }

  private:
    unsigned int pipelineLowerLimit_;

    std::string onSegment(const boost::shared_ptr<const EventSegment> &ev) override;
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

		std::string str() const override { return kStateFetching; }
        int toInt() override { return (int)StateId::Fetching; }
        
	private:
		std::string onSegment(const boost::shared_ptr<const EventSegment>& ev) override;
	};

//******************************************************************************
std::string
PipelineControlEvent::toString() const
{
	switch (e_)
	{
        case PipelineControlEvent::Init: return "Init";
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
PipelineControlStateMachine::defaultConsumerStatesMap(const boost::shared_ptr<PipelineControlStateMachine::Struct> &ctrl)
{
    return 
    {
        {kStateIdle, boost::make_shared<IdleAudio>(ctrl)},
        {kStateBootstrapping, boost::make_shared<BootstrappingAudio>(ctrl)},
        {kStateAdjusting, boost::make_shared<Adjusting>(ctrl)},
        {kStateFetching, boost::make_shared<Fetching>(ctrl)}
    };
}

PipelineControlStateMachine::StatesMap
PipelineControlStateMachine::videoConsumerStatesMap(const boost::shared_ptr<PipelineControlStateMachine::Struct>& ctrl)
{
    return 
    {
        {kStateIdle, boost::make_shared<IdleVideo>(ctrl)},
        {kStateBootstrapping, boost::make_shared<BootstrappingVideo>(ctrl)},
        {kStateAdjusting, boost::make_shared<Adjusting>(ctrl)},
        {kStateFetching, boost::make_shared<Fetching>(ctrl)}
    };
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
		(STATE_TRANSITION(kStateIdle, Start), 					kStateBootstrapping)
		(STATE_TRANSITION(kStateBootstrapping, Reset), 		    kStateIdle)
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
        case PipelineControlEvent::Init: 
            return onInit(boost::dynamic_pointer_cast<const EventInit>(ev));
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
