//
// pipeline-control-state-machine.hpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeline_control_state_machine_h__
#define __pipeline_control_state_machine_h__
#if 0
#include <ndn-cpp/name.hpp>

#include "ndnrtc-object.hpp"
#include "name-components.hpp"
#include "../include/remote-stream.hpp"

namespace ndn
{
class Interest;
}

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class PipelineControlState;
class DrdEstimator;
class IPipeliner;
class IInterestControl;
class ILatencyControl;
class IPlayoutControl;
class IBuffer;
class PipelineControl;
class WireSegment;
class SampleEstimator;
struct Mutable;
template <typename T>
class NetworkDataT;
typedef NetworkDataT<Mutable> NetworkDataAlias;

extern const std::string kStateIdle;
extern const std::string kStateBootstrapping;
extern const std::string kStateAdjusting;
extern const std::string kStateFetching;

/**
 * Base class for pipeline control events
 */
class PipelineControlEvent
{
  public:
    typedef enum _Type {
        Start,
        Reset,
        Starvation,
        Segment,
        Timeout,
        Nack
    } Type;

    PipelineControlEvent(Type e) : e_(e) {}
    PipelineControlEvent::Type getType() const { return e_; }
    virtual ~PipelineControlEvent() {}

    virtual std::string toString() const;

  private:
    PipelineControlEvent::Type e_;
};

class EventSegment : public PipelineControlEvent
{
  public:
    EventSegment(const std::shared_ptr<const WireSegment> &segment) 
        : PipelineControlEvent(PipelineControlEvent::Segment), segment_(segment) {}

    std::shared_ptr<const WireSegment> getSegment() const { return segment_; }

  private:
    std::shared_ptr<const WireSegment> segment_;
};

class EventStart : public PipelineControlEvent
{
   public:
     EventStart(const std::shared_ptr<NetworkDataAlias> &data) 
         : PipelineControlEvent(PipelineControlEvent::Start), data_(data){};

     const std::shared_ptr<NetworkDataAlias> getMetadata() const { return data_; }

   private:
     std::shared_ptr<NetworkDataAlias> data_;
};

class EventTimeout : public PipelineControlEvent
{
  public:
    EventTimeout(const NamespaceInfo &info, const std::shared_ptr<const ndn::Interest> &i) 
        : PipelineControlEvent(PipelineControlEvent::Timeout), info_(info), interest_(i) {}

    const NamespaceInfo &getInfo() const { return info_; }
    const std::shared_ptr<const ndn::Interest> getInterest() const { return interest_; }

  private:
    NamespaceInfo info_;
    const std::shared_ptr<const ndn::Interest> interest_;
};

class EventNack : public PipelineControlEvent
{
  public:
    EventNack(const NamespaceInfo &info, int reason, const std::shared_ptr<const ndn::Interest> &i) 
        : PipelineControlEvent(PipelineControlEvent::Nack), info_(info), reason_(reason), interest_(i) {}

    const NamespaceInfo &getInfo() const { return info_; }
    int getReason() const { return reason_; }
    const std::shared_ptr<const ndn::Interest> getInterest() const { return interest_; }

  private:
    NamespaceInfo info_;
    int reason_;
    const std::shared_ptr<const ndn::Interest> interest_;
};

class EventStarvation : public PipelineControlEvent
{
  public:
    EventStarvation(unsigned int duration) 
        : PipelineControlEvent(PipelineControlEvent::Starvation), duration_(duration) {}

    unsigned int getDuration() const { return duration_; }

  private:
    unsigned int duration_;
};

class IPipelineControlStateMachineObserver;

/**
 * Implements simple state machine for pipeline control:
 *
 * 	IDLE -------(start)-----> BOOTSTRAPPING -------(init)-------+
 *                                                              |
 *                                                              |
 *              +---<<<---(minimized pipeline)---<<<---+        |
 *              |                                      |        |
 *          FETCHING                                ADJUSTING <-+
 *              |                                      |
 *              +--->>>---(   out of sync    )--->>>---+
 *
 * additional notes:
 * - from any state, segmentStarvation() brings machine into BOOTSTRAPPING state
 * - timeout in BOOTSTRAPPING causes re-entering of this state
 */
class PipelineControlStateMachine : public NdnRtcComponent
{
  public:
    typedef std::map<std::string, std::shared_ptr<PipelineControlState>>
        StatesMap;
    typedef struct _Struct
    {
        _Struct(const ndn::Name threadPrefix) : threadPrefix_(threadPrefix) {}

        const ndn::Name threadPrefix_;
        std::shared_ptr<DrdEstimator> drdEstimator_;
        std::shared_ptr<IBuffer> buffer_;
        std::shared_ptr<IPipeliner> pipeliner_;
        std::shared_ptr<IInterestControl> interestControl_;
        std::shared_ptr<ILatencyControl> latencyControl_;
        std::shared_ptr<IPlayoutControl> playoutControl_;
        std::shared_ptr<statistics::StatisticsStorage> sstorage_;
        std::shared_ptr<SampleEstimator> sampleEstimator_;
    } Struct;

    ~PipelineControlStateMachine();

    std::string getState() const;
    std::shared_ptr<PipelineControlState> currentState() const { return currentState_; }
    void dispatch(const std::shared_ptr<const PipelineControlEvent> &ev);

    // not thread-safe! should be called on the same thread as dispatch(...)
    void attach(IPipelineControlStateMachineObserver *);
    // not thread-safe! should be called on the same thread as dispatch(...)
    void detach(IPipelineControlStateMachineObserver *);

    static PipelineControlStateMachine defaultStateMachine(Struct ctrl);
    static PipelineControlStateMachine videoStateMachine(Struct ctrl);

    /**
     * This state machine is for video streams (currently) and similar to the 
     * videoStateMachine, but has no Adjusting state and different Bootstrapping
     * state which does not use metadata for bootstrapping but actual frame numbers.
     */
    static PipelineControlStateMachine playbackDrivenStateMachine(const RemoteVideoStream::FetchingRuleSet& ruleset, 
                                                                  Struct ctrl);

  private:
    typedef std::pair<std::string, PipelineControlEvent::Type> StateEventPair;
    typedef std::map<StateEventPair, std::string> TransitionMap;

    std::shared_ptr<Struct> ppCtrl_;
    std::map<std::string, std::shared_ptr<PipelineControlState>> states_;
    TransitionMap stateMachineTable_;
    std::shared_ptr<PipelineControlState> currentState_;
    int64_t lastEventTimestamp_;
    std::vector<IPipelineControlStateMachineObserver *> observers_;

    PipelineControlStateMachine(const std::shared_ptr<Struct> &ctrl,
                                StatesMap statesMap);

    bool transition(const std::shared_ptr<const PipelineControlEvent> &ev);
    void switchToState(const std::shared_ptr<PipelineControlState> &state,
                       const std::shared_ptr<const PipelineControlEvent> &event);

    static StatesMap defaultConsumerStatesMap(const std::shared_ptr<PipelineControlStateMachine::Struct> &);
    static StatesMap videoConsumerStatesMap(const std::shared_ptr<PipelineControlStateMachine::Struct> &);
    static StatesMap playbackDrivenConsumerStatesMap(const RemoteVideoStream::FetchingRuleSet& ruleset,
                                                     const std::shared_ptr<PipelineControlStateMachine::Struct> &);
};

class IPipelineControlStateMachineObserver
{
  public:
    virtual void onStateMachineChangedState(const std::shared_ptr<const PipelineControlEvent> &,
                                            std::string newState) = 0;
    // called whenever received event didn't trigger any state change
    virtual void onStateMachineReceivedEvent(const std::shared_ptr<const PipelineControlEvent> &,
                                             std::string state) = 0;
};

/**
 * Base class for pipeline control states
 */
class PipelineControlState
{
  public:
    typedef enum _StateId {
        Unknown = 0,
        Idle = 1,
        Bootstrapping = 2,
        Adjusting = 3,
        Fetching = 4
    } StateId;

    PipelineControlState(const std::shared_ptr<PipelineControlStateMachine::Struct> &ctrl) : ctrl_(ctrl) {}

    virtual std::string str() const = 0;

    /**
     * Called when state is entered
     * @param ev Event that caused entering this state
     */
    virtual void enter(const std::shared_ptr<const PipelineControlEvent> &ev) {}

    /**
     * Called when state is exited
     */
    virtual void exit() {}

    /**
     * Called when upon new event
     * @param event State machine event
     * @return Next state transition to
     */
    virtual std::string dispatchEvent(const std::shared_ptr<const PipelineControlEvent> &ev);

    bool operator==(const PipelineControlState &other) const
    {
        return str() == other.str();
    }

    virtual int toInt() { return (int)StateId::Unknown; }

  protected:
    std::shared_ptr<PipelineControlStateMachine::Struct> ctrl_;

    virtual std::string onStart(const std::shared_ptr<const PipelineControlEvent> &)
    {
        return str();
    }
    virtual std::string onReset(const std::shared_ptr<const PipelineControlEvent> &ev)
    {
        return str();
    }
    virtual std::string onStarvation(const std::shared_ptr<const EventStarvation> &ev)
    {
        return str();
    }
    virtual std::string onTimeout(const std::shared_ptr<const EventTimeout> &ev)
    {
        return str();
    }
    virtual std::string onNack(const std::shared_ptr<const EventNack> &ev)
    {
        return str();
    }
    virtual std::string onSegment(const std::shared_ptr<const EventSegment> &ev)
    {
        return str();
    }
};
}

#endif
#endif
