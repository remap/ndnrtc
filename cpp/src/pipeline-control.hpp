//
// pipeline-control.hpp
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeline_control_h__
#define __pipeline_control_h__

#include <stdlib.h>
#include <boost/signals2.hpp>

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

namespace v4 {
    /**
     * PipelineControl class implements timing logic for pipelining outstanding
     * requests. It does not generate requests on its own, but rather notifies
     * subscribers on when new requests must be generated.
     * Client code is also responsible of notifying ("pulsing") PipelineControl
     * whenever previous outstanding request has been processed. When pulsed,
     * PipelineControl may trigger a "new request" event or hold
     * off (triggering "skip pulse" event), according to the implementation.
     * In its' simplest form, PipelineControl can be implemented with a fixed
     * "window" approach. More complicated implementations may include dynamic
     * windows.
     */
    class PipelineControl : public NdnRtcComponent
                          // , public statistics::StatObject
    {
    public:
        PipelineControl(uint32_t w);
        virtual ~PipelineControl();

        /**
         * Resets internal structures and intializes mechanism.
         */
        void reset();

        /**
         * Must be called any time client code wants to notify that outstanding
         * request has been processed.
         * Must be also called to initialize mechanism for the first time (or
         * any time after calling "reset"). This will trigger multiple "new request"
         * events, depending on implementation.
         */
        void pulse();

        /**
         * Triggered any time when PipelineControl thinks its' time to generate
         * new outstanding request.
         */
        boost::signals2::signal<void()> onNewRequest;

        /**
         * Triggered any time when PipelineControl skips a pulse.
         */
        boost::signals2::signal<void()> onSkipPulse;

        /**
         * Return number of outstanding requests.
         */
        uint32_t getNumOutstanding() const;

        /**
         * Return current W size
         */
        uint32_t getWSize() const { return wSize_; }

        /**
         * Set current W size
         */
        void setWSize(uint32_t w) { wSize_ = w; }

    private:
        uint32_t wSize_;
        int32_t nOutstanding_;
        uint64_t pulseCount_;
    };
}
#if 0
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

    void start(const std::shared_ptr<NetworkDataAlias>&);
    void stop();

    void segmentArrived(const std::shared_ptr<WireSegment> &);
    void segmentRequestTimeout(const NamespaceInfo &,
                               const std::shared_ptr<const ndn::Interest> &);
    void segmentNack(const NamespaceInfo &, int,
                     const std::shared_ptr<const ndn::Interest> &);
    void segmentStarvation();

    bool needPipelineAdjustment(const PipelineAdjust &);
    void setLogger(std::shared_ptr<ndnlog::new_api::Logger> logger);

    static PipelineControl defaultPipelineControl(const ndn::Name &threadPrefix,
                                                  const std::shared_ptr<DrdEstimator> drdEstimator,
                                                  const std::shared_ptr<IBuffer> buffer,
                                                  const std::shared_ptr<IPipeliner> pipeliner,
                                                  const std::shared_ptr<IInterestControl> interestControl,
                                                  const std::shared_ptr<ILatencyControl> latencyControl,
                                                  const std::shared_ptr<IPlayoutControl> playoutControl,
                                                  const std::shared_ptr<SampleEstimator> sampleEstimator,
                                                  const std::shared_ptr<statistics::StatisticsStorage> &storage);
    static PipelineControl videoPipelineControl(const ndn::Name &threadPrefix,
                                                const std::shared_ptr<DrdEstimator> drdEstimator,
                                                const std::shared_ptr<IBuffer> buffer,
                                                const std::shared_ptr<IPipeliner> pipeliner,
                                                const std::shared_ptr<IInterestControl> interestControl,
                                                const std::shared_ptr<ILatencyControl> latencyControl,
                                                const std::shared_ptr<IPlayoutControl> playoutControl,
                                                const std::shared_ptr<SampleEstimator> sampleEstimator,
                                                const std::shared_ptr<statistics::StatisticsStorage> &storage);

    static PipelineControl seedPipelineControl(const RemoteVideoStream::FetchingRuleSet& ruleset,
                                                const ndn::Name &threadPrefix,
                                                const std::shared_ptr<DrdEstimator> drdEstimator,
                                                const std::shared_ptr<IBuffer> buffer,
                                                const std::shared_ptr<IPipeliner> pipeliner,
                                                const std::shared_ptr<IInterestControl> interestControl,
                                                const std::shared_ptr<ILatencyControl> latencyControl,
                                                const std::shared_ptr<IPlayoutControl> playoutControl,
                                                const std::shared_ptr<SampleEstimator> sampleEstimator,
                                                const std::shared_ptr<statistics::StatisticsStorage> &storage);

  private:
    PipelineControlStateMachine machine_;
    std::shared_ptr<IInterestControl> interestControl_;
    std::shared_ptr<IPipeliner> pipeliner_;
    // TODO: clean this up. bootstrapping process is messed up
    // making quick fixes for EB workshop.
    std::shared_ptr<NetworkDataAlias> metadata_;

    PipelineControl(const std::shared_ptr<statistics::StatisticsStorage> &statStorage,
                    const PipelineControlStateMachine &machine,
                    const std::shared_ptr<IInterestControl> &interestControl,
                    const std::shared_ptr<IPipeliner> pipeliner_);

    void onStateMachineChangedState(const std::shared_ptr<const PipelineControlEvent> &,
                                    std::string);
    void onStateMachineReceivedEvent(const std::shared_ptr<const PipelineControlEvent> &,
                                     std::string);
    void onRetransmissionRequired(const std::vector<std::shared_ptr<const ndn::Interest>> &interests);
};
#endif
}

#endif
