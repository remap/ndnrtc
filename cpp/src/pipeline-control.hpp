//
// pipeline-control.hpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeline_control_h__
#define __pipeline_control_h__

#include <stdlib.h>

#include "ndnrtc-object.hpp"
#include "latency-control.hpp"
#include "segment-controller.hpp"
#include "pipeline-control-state-machine.hpp"
#include "pipeliner.hpp"
#include "rtx-controller.hpp"
#include "../include/remote-stream.hpp"

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class IPipeliner;
class IInterestControl;
class IPlayoutControl;
class IBuffer;
class PipelineControlStateMachine;
class SampleEstimator;
template <typename T>
class NetworkDataT;
typedef NetworkDataT<Mutable> NetworkDataAlias;

/**
 * PipelineControl class implements functionality of a consumer by 
 * dispatching events to consumer state machine and adjusting interest 
 * pipeline size using InterestControl class.
 */
class PipelineControl : public NdnRtcComponent,
                        public ILatencyControlObserver,
                        public ISegmentControllerObserver,
                        public IRtxObserver,
                        public IPipelineControlStateMachineObserver,
                        public statistics::StatObject
{
  public:
    ~PipelineControl();

    void start();
    void stop();

    void segmentArrived(const boost::shared_ptr<WireSegment> &);
    void segmentRequestTimeout(const NamespaceInfo &, 
                               const boost::shared_ptr<const ndn::Interest> &);
    void segmentNack(const NamespaceInfo &, int,
                     const boost::shared_ptr<const ndn::Interest> &);
    void segmentStarvation();

    bool needPipelineAdjustment(const PipelineAdjust &);
    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

    static PipelineControl defaultPipelineControl(const ndn::Name &threadPrefix,
                                                  const boost::shared_ptr<DrdEstimator> drdEstimator,
                                                  const boost::shared_ptr<IBuffer> buffer,
                                                  const boost::shared_ptr<IPipeliner> pipeliner,
                                                  const boost::shared_ptr<IInterestControl> interestControl,
                                                  const boost::shared_ptr<ILatencyControl> latencyControl,
                                                  const boost::shared_ptr<IPlayoutControl> playoutControl,
                                                  const boost::shared_ptr<SampleEstimator> sampleEstimator,
                                                  const boost::shared_ptr<statistics::StatisticsStorage> &storage);
    static PipelineControl videoPipelineControl(const ndn::Name &threadPrefix,
                                                const boost::shared_ptr<DrdEstimator> drdEstimator,
                                                const boost::shared_ptr<IBuffer> buffer,
                                                const boost::shared_ptr<IPipeliner> pipeliner,
                                                const boost::shared_ptr<IInterestControl> interestControl,
                                                const boost::shared_ptr<ILatencyControl> latencyControl,
                                                const boost::shared_ptr<IPlayoutControl> playoutControl,
                                                const boost::shared_ptr<SampleEstimator> sampleEstimator,
                                                const boost::shared_ptr<statistics::StatisticsStorage> &storage);

    static PipelineControl seedPipelineControl(const RemoteVideoStream::FetchingRuleSet& ruleset,
                                                const ndn::Name &threadPrefix,
                                                const boost::shared_ptr<DrdEstimator> drdEstimator,
                                                const boost::shared_ptr<IBuffer> buffer,
                                                const boost::shared_ptr<IPipeliner> pipeliner,
                                                const boost::shared_ptr<IInterestControl> interestControl,
                                                const boost::shared_ptr<ILatencyControl> latencyControl,
                                                const boost::shared_ptr<IPlayoutControl> playoutControl,
                                                const boost::shared_ptr<SampleEstimator> sampleEstimator,
                                                const boost::shared_ptr<statistics::StatisticsStorage> &storage);

  private:
    PipelineControlStateMachine machine_;
    boost::shared_ptr<IInterestControl> interestControl_;
    boost::shared_ptr<IPipeliner> pipeliner_;

    PipelineControl(const boost::shared_ptr<statistics::StatisticsStorage> &statStorage,
                    const PipelineControlStateMachine &machine,
                    const boost::shared_ptr<IInterestControl> &interestControl,
                    const boost::shared_ptr<IPipeliner> pipeliner_);

    void onStateMachineChangedState(const boost::shared_ptr<const PipelineControlEvent> &,
                                    std::string);
    void onStateMachineReceivedEvent(const boost::shared_ptr<const PipelineControlEvent> &,
                                     std::string);
    void onRetransmissionRequired(const std::vector<boost::shared_ptr<const ndn::Interest>> &interests);
};
}

#endif