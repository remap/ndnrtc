// 
// pipeline-control-state-machine.h
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeline_control_state_machine_h__
#define __pipeline_control_state_machine_h__

#include <ndn-cpp/name.hpp>

#include "ndnrtc-object.h"
#include "name-components.h"

namespace ndn {
	class Interest;
}

namespace ndnrtc {
	class PipelineControlState;
	class IPipeliner;
	class IInterestControl;
	class ILatencyControl;
	class PipelineControl;
	class WireSegment;

	extern const std::string kStateIdle;
	extern const std::string kStateWaitForRightmost;
	extern const std::string kStateWaitForInitial;
	extern const std::string kStateChasing;
	extern const std::string kStateAdjusting;
	extern const std::string kStateFetching;

	/**
	 * Base class for pipeline control events
	 */
	class PipelineControlEvent {
	public:
		typedef enum _Type {
			Start,
			Reset,
			Starvation,
			Segment,
			Timeout
		} Type;

		PipelineControlEvent(Type e):e_(e){}
		PipelineControlEvent::Type getType() const { return e_; }
		virtual ~PipelineControlEvent(){}

		virtual std::string toString() const;

	private:
		PipelineControlEvent::Type e_;
	};

	class EventSegment : public PipelineControlEvent {
	public:
		EventSegment(const boost::shared_ptr<const WireSegment>& segment):
			PipelineControlEvent(PipelineControlEvent::Segment),segment_(segment){}
		boost::shared_ptr<const WireSegment> getSegment() const { return segment_; }
	private:
		boost::shared_ptr<const WireSegment> segment_;
	};

	class EventTimeout : public PipelineControlEvent {
	public:
		EventTimeout(const NamespaceInfo& info):
			PipelineControlEvent(PipelineControlEvent::Timeout), info_(info){}
		const NamespaceInfo& getInfo() const { return info_; }
	private:
		NamespaceInfo info_;
	};

	class EventStarvation : public PipelineControlEvent {
	public:
		EventStarvation(unsigned int duration):
			PipelineControlEvent(PipelineControlEvent::Starvation), duration_(duration){}
		unsigned int getDuration() const { return duration_; }
	private:
		unsigned int duration_;
	};

	/**
	 * Implements simple state machine for pipeline control:
	 *
	 * 	IDLE --|start|--> WAITFORRIGHTMOST --|segment|--> WAITFORINITIAL -|segment|-+
	 *																				|
	 *					+-- ADJUSTING <--|latest data arrive|-- CHASING <-----------+
	 *					|
	 *					+--|minimized pipeline|--> FETCHING
	 *
	 * additional notes:
	 * - from any state, segmentStarvation() brings machine into WAITFORRIGHTMOST state
	 * - timeout in WAITFORRIGHTMOST causes re-entering this state with expression of 
	 *		RM interest
	 *
	 */
	class PipelineControlStateMachine : public NdnRtcComponent {
	public:
		typedef struct _Struct {
			_Struct(const ndn::Name threadPrefix):threadPrefix_(threadPrefix){}

			const ndn::Name threadPrefix_;
			boost::shared_ptr<IPipeliner> pipeliner_;
			boost::shared_ptr<IInterestControl> interestControl_;
			boost::shared_ptr<ILatencyControl> latencyControl_;
		} Struct;

		PipelineControlStateMachine(Struct ctrl);
		~PipelineControlStateMachine();

		std::string getState() const;
		boost::shared_ptr<PipelineControlState> currentState() const { return currentState_; }
		void dispatch(const boost::shared_ptr<const PipelineControlEvent>& ev);

	private:
		typedef std::map<std::pair<std::string, PipelineControlEvent::Type>, 
			std::string> TransitionMap;

		Struct ppCtrl_;
		std::map<std::string, boost::shared_ptr<PipelineControlState>> states_;
		TransitionMap stateMachineTable_;
		boost::shared_ptr<PipelineControlState> currentState_;
		int64_t lastEventTimestamp_;

		bool transition(const boost::shared_ptr<const PipelineControlEvent>& ev);
		void switchToState(const std::string& state);
	};
}

#endif