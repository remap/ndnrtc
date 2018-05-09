//
// pipeline-control-state-machine.hpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeline_control_state_machine_h__
#define __pipeline_control_state_machine_h__

#include <ndn-cpp/name.hpp>

#include "ndnrtc-object.hpp"
#include "name-components.hpp"

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
    EventSegment(const boost::shared_ptr<const WireSegment> &segment) 
        : PipelineControlEvent(PipelineControlEvent::Segment), segment_(segment) {}

    boost::shared_ptr<const WireSegment> getSegment() const { return segment_; }

  private:
    boost::shared_ptr<const WireSegment> segment_;
};

// class EventInit : public PipelineControlEvent
// {
//   public:
//     EventInit(const boost::shared_ptr<NetworkDataAlias> &data) 
//         : PipelineControlEvent(PipelineControlEvent::Init), data_(data){};

//     const boost::shared_ptr<NetworkDataAlias> getNetworkData() const { return data_; }

//   private:
//     boost::shared_ptr<NetworkDataAlias> data_;
// };

class EventTimeout : public PipelineControlEvent
{
  public:
    EventTimeout(const NamespaceInfo &info) 
        : PipelineControlEvent(PipelineControlEvent::Timeout), info_(info) {}

    const NamespaceInfo &getInfo() const { return info_; }

  private:
    NamespaceInfo info_;
};

class EventNack : public PipelineControlEvent
{
  public:
    EventNack(const NamespaceInfo &info, int reason) 
        : PipelineControlEvent(PipelineControlEvent::Nack), info_(info), reason_(reason) {}

    const NamespaceInfo &getInfo() const { return info_; }
    int getReason() const { return reason_; }

  private:
    NamespaceInfo info_;
    int reason_;
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
    typedef std::map<std::string, boost::shared_ptr<PipelineControlState>>
        StatesMap;
    typedef struct _Struct
    {
        _Struct(const ndn::Name threadPrefix) : threadPrefix_(threadPrefix) {}

        const ndn::Name threadPrefix_;
        boost::shared_ptr<DrdEstimator> drdEstimator_;
        boost::shared_ptr<IBuffer> buffer_;
        boost::shared_ptr<IPipeliner> pipeliner_;
        boost::shared_ptr<IInterestControl> interestControl_;
        boost::shared_ptr<ILatencyControl> latencyControl_;
        boost::shared_ptr<IPlayoutControl> playoutControl_;
        boost::shared_ptr<statistics::StatisticsStorage> sstorage_;
        boost::shared_ptr<SampleEstimator> sampleEstimator_;
    } Struct;

    ~PipelineControlStateMachine();

    std::string getState() const;
    boost::shared_ptr<PipelineControlState> currentState() const { return currentState_; }
    void dispatch(const boost::shared_ptr<const PipelineControlEvent> &ev);

    // not thread-safe! should be called on the same thread as dispatch(...)
    void attach(IPipelineControlStateMachineObserver *);
    // not thread-safe! should be called on the same thread as dispatch(...)
    void detach(IPipelineControlStateMachineObserver *);

    static PipelineControlStateMachine defaultStateMachine(Struct ctrl);
    static PipelineControlStateMachine videoStateMachine(Struct ctrl);

  private:
    typedef std::pair<std::string, PipelineControlEvent::Type> StateEventPair;
    typedef std::map<StateEventPair, std::string> TransitionMap;

    boost::shared_ptr<Struct> ppCtrl_;
    std::map<std::string, boost::shared_ptr<PipelineControlState>> states_;
    TransitionMap stateMachineTable_;
    boost::shared_ptr<PipelineControlState> currentState_;
    int64_t lastEventTimestamp_;
    std::vector<IPipelineControlStateMachineObserver *> observers_;

    PipelineControlStateMachine(const boost::shared_ptr<Struct> &ctrl,
                                StatesMap statesMap);

    bool transition(const boost::shared_ptr<const PipelineControlEvent> &ev);
    void switchToState(const boost::shared_ptr<PipelineControlState> &state,
                       const boost::shared_ptr<const PipelineControlEvent> &event);

    static StatesMap defaultConsumerStatesMap(const boost::shared_ptr<PipelineControlStateMachine::Struct> &);
    static StatesMap videoConsumerStatesMap(const boost::shared_ptr<PipelineControlStateMachine::Struct> &);
};

class IPipelineControlStateMachineObserver
{
  public:
    virtual void onStateMachineChangedState(const boost::shared_ptr<const PipelineControlEvent> &,
                                            std::string newState) = 0;
    // called whenever received event didn't trigger any state change
    virtual void onStateMachineReceivedEvent(const boost::shared_ptr<const PipelineControlEvent> &,
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

    PipelineControlState(const boost::shared_ptr<PipelineControlStateMachine::Struct> &ctrl) : ctrl_(ctrl) {}

    virtual std::string str() const = 0;

    /**
     * Called when state is entered
     */
    virtual void enter() {}

    /**
     * Called when state is exited
     */
    virtual void exit() {}

    /**
     * Called when upon new event
     * @param event State machine event
     * @return Next state transition to
     */
    virtual std::string dispatchEvent(const boost::shared_ptr<const PipelineControlEvent> &ev);

    bool operator==(const PipelineControlState &other) const
    {
        return str() == other.str();
    }

    virtual int toInt() { return (int)StateId::Unknown; }

  protected:
    boost::shared_ptr<PipelineControlStateMachine::Struct> ctrl_;

    virtual std::string onStart(const boost::shared_ptr<const PipelineControlEvent> &)
    {
        return str();
    }
    virtual std::string onReset(const boost::shared_ptr<const PipelineControlEvent> &ev)
    {
        return str();
    }
    virtual std::string onStarvation(const boost::shared_ptr<const EventStarvation> &ev)
    {
        return str();
    }
    virtual std::string onTimeout(const boost::shared_ptr<const EventTimeout> &ev)
    {
        return str();
    }
    virtual std::string onNack(const boost::shared_ptr<const EventNack> &ev)
    {
        return str();
    }
    virtual std::string onSegment(const boost::shared_ptr<const EventSegment> &ev)
    {
        return str();
    }
};
}

#endif
