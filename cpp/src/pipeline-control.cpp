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
using namespace ndnrtc::statistics;

//******************************************************************************
PipelineControl PipelineControl::defaultPipelineControl(const ndn::Name &threadPrefix,
                                                        const boost::shared_ptr<DrdEstimator> drdEstimator,
                                                        const boost::shared_ptr<IBuffer> buffer,
                                                        const boost::shared_ptr<IPipeliner> pipeliner,
                                                        const boost::shared_ptr<IInterestControl> interestControl,
                                                        const boost::shared_ptr<ILatencyControl> latencyControl,
                                                        const boost::shared_ptr<IPlayoutControl> playoutControl,
                                                        const boost::shared_ptr<SampleEstimator> sampleEstimator,
                                                        const boost::shared_ptr<statistics::StatisticsStorage> &storage)
{
    PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.drdEstimator_ = drdEstimator;
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pipeliner;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
    ctrl.sampleEstimator_ = sampleEstimator;
    return PipelineControl(storage, PipelineControlStateMachine::defaultStateMachine(ctrl),
                           interestControl, pipeliner);
}

PipelineControl PipelineControl::videoPipelineControl(const ndn::Name &threadPrefix,
                                                      const boost::shared_ptr<DrdEstimator> drdEstimator,
                                                      const boost::shared_ptr<IBuffer> buffer,
                                                      const boost::shared_ptr<IPipeliner> pipeliner,
                                                      const boost::shared_ptr<IInterestControl> interestControl,
                                                      const boost::shared_ptr<ILatencyControl> latencyControl,
                                                      const boost::shared_ptr<IPlayoutControl> playoutControl,
                                                      const boost::shared_ptr<SampleEstimator> sampleEstimator,
                                                      const boost::shared_ptr<statistics::StatisticsStorage> &storage)
{
    PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.drdEstimator_ = drdEstimator;
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pipeliner;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
    ctrl.sampleEstimator_ = sampleEstimator;
    return PipelineControl(storage, PipelineControlStateMachine::videoStateMachine(ctrl),
                           interestControl, pipeliner);
}

PipelineControl PipelineControl::seedPipelineControl(const RemoteVideoStream::FetchingRuleSet& ruleset,
                                                const ndn::Name &threadPrefix,
                                                const boost::shared_ptr<DrdEstimator> drdEstimator,
                                                const boost::shared_ptr<IBuffer> buffer,
                                                const boost::shared_ptr<IPipeliner> pipeliner,
                                                const boost::shared_ptr<IInterestControl> interestControl,
                                                const boost::shared_ptr<ILatencyControl> latencyControl,
                                                const boost::shared_ptr<IPlayoutControl> playoutControl,
                                                const boost::shared_ptr<SampleEstimator> sampleEstimator,
                                                const boost::shared_ptr<statistics::StatisticsStorage> &storage)
{
    PipelineControlStateMachine::Struct ctrl(threadPrefix);
    ctrl.drdEstimator_ = drdEstimator;
    ctrl.buffer_ = buffer;
    ctrl.pipeliner_ = pipeliner;
    ctrl.interestControl_ = interestControl;
    ctrl.latencyControl_ = latencyControl;
    ctrl.playoutControl_ = playoutControl;
    ctrl.sstorage_ = storage;
    ctrl.sampleEstimator_ = sampleEstimator;
    return PipelineControl(storage, PipelineControlStateMachine::playbackDrivenStateMachine(ruleset, ctrl),
                           interestControl, pipeliner);
}

//******************************************************************************
PipelineControl::PipelineControl(const boost::shared_ptr<StatisticsStorage> &statStorage,
                                 const PipelineControlStateMachine &machine,
                                 const boost::shared_ptr<IInterestControl> &interestControl,
                                 const boost::shared_ptr<IPipeliner> pipeliner)
    : StatObject(statStorage),
      machine_(machine),
      interestControl_(interestControl),
      pipeliner_(pipeliner)
{
    description_ = "pipeline-control";
}

PipelineControl::~PipelineControl()
{
}

void PipelineControl::start(const boost::shared_ptr<NetworkDataAlias>& metadata)
{
    if (machine_.getState() != kStateIdle)
        throw std::runtime_error("Can't start Pipeline Control as it has been "
                                 "started already. Use reset() and start() to restart.");

    metadata_ = metadata;
    machine_.attach(this);
    machine_.dispatch(boost::make_shared<EventStart>(metadata));

    LogDebugC << "started." << std::endl;
}

void PipelineControl::stop()
{
    machine_.detach(this);
    machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Reset));
    LogDebugC << "stopped" << std::endl;
}

void PipelineControl::segmentArrived(const boost::shared_ptr<WireSegment> &s)
{
    if (s->getSampleClass() == SampleClass::Key ||
        s->getSampleClass() == SampleClass::Delta ||
        s->getSegmentClass() == SegmentClass::Meta || // TODO: do we still need class Meta here?
        s->getSegmentClass() == SegmentClass::Pointer)
    {
        machine_.dispatch(boost::make_shared<EventSegment>(s));
    }
}

void PipelineControl::segmentRequestTimeout(const NamespaceInfo &n, 
                                            const boost::shared_ptr<const ndn::Interest> &interest)
{
    machine_.dispatch(boost::make_shared<EventTimeout>(n, interest));
}

void PipelineControl::segmentNack(const NamespaceInfo &n, int reason,
                                   const boost::shared_ptr<const ndn::Interest> &interest)
{
    machine_.dispatch(boost::make_shared<EventNack>(n, reason, interest));
}

void PipelineControl::segmentStarvation()
{
    machine_.dispatch(boost::make_shared<EventStarvation>(500));
    machine_.dispatch(boost::make_shared<PipelineControlEvent>(PipelineControlEvent::Start));
}

bool PipelineControl::needPipelineAdjustment(const PipelineAdjust &cmd)
{
    if (cmd == PipelineAdjust::IncreasePipeline ||
        cmd == PipelineAdjust::DecreasePipeline)
    {
        if (cmd == PipelineAdjust::IncreasePipeline)
            interestControl_->burst();
        if (cmd == PipelineAdjust::DecreasePipeline)
            interestControl_->withhold();
        return true;
    }
    return false;
}

void PipelineControl::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{
    NdnRtcComponent::setLogger(logger);
    machine_.setLogger(logger);
}

#pragma mark - private
void PipelineControl::onStateMachineChangedState(const boost::shared_ptr<const PipelineControlEvent> &trigger,
                                                 std::string newState)
{
    // if new state is idle - reset the machine
    if (newState == kStateIdle &&
        trigger->getType() != PipelineControlEvent::Type::Reset)
    {
        LogInfoC << "state machine reverted to Idle. starting over..." << std::endl;

        (*statStorage_)[statistics::Indicator::RebufferingsNum]++;

        stop();
        start(metadata_);
    }
}

void PipelineControl::onStateMachineReceivedEvent(const boost::shared_ptr<const PipelineControlEvent> &trigger,
                                                  std::string currentState)
{
}

void PipelineControl::onRetransmissionRequired(const std::vector<boost::shared_ptr<const ndn::Interest>> &interests)
{
    if (machine_.currentState()->toInt() >= PipelineControlState::Bootstrapping)
    {
        LogDebugC << "retransmission for " << interests[0]->getName().getPrefix(-1) << std::endl;
        pipeliner_->express(interests, true);
    }
}
