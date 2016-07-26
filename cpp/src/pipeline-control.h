// 
// pipeline-control.h
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __pipeline_control_h__
#define __pipeline_control_h__

#include <stdlib.h>

#include "ndnrtc-object.h"
#include "latency-control.h"
#include "segment-controller.h"
#include "pipeline-control-state-machine.h"

namespace ndnrtc {
	class IPipeliner;
	class IInterestControl;
	class IPlayoutControl;
    class IBuffer;
	class PipelineControlStateMachine;

	/**
	 * PipelineControl class implements functionality of a consumer by 
	 * dispatching events to consumer state machine and adjusting interest 
	 * pipeline size using InterestControl class.
	 */
	class PipelineControl : public NdnRtcComponent,
							public ILatencyControlObserver,
							public ISegmentControllerObserver
	{
	public:
		~PipelineControl(){}

		void start();
		void stop();

		void segmentArrived(const boost::shared_ptr<WireSegment>&);
		void segmentRequestTimeout(const NamespaceInfo&);
		void segmentStarvation();

		bool needPipelineAdjustment(const PipelineAdjust&);
        void setLogger(ndnlog::new_api::Logger* logger);

		static PipelineControl defaultPipelineControl(const ndn::Name& threadPrefix,
            const boost::shared_ptr<IBuffer> buffer,
			const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl,
			const boost::shared_ptr<IPlayoutControl> playoutControl);
		static PipelineControl videoPipelineControl(const ndn::Name& threadPrefix,
            const boost::shared_ptr<IBuffer> buffer,
			const boost::shared_ptr<IPipeliner> pipeliner,
			const boost::shared_ptr<IInterestControl> interestControl,
			const boost::shared_ptr<ILatencyControl> latencyControl,
			const boost::shared_ptr<IPlayoutControl> playoutControl);
	
	private:
		PipelineControlStateMachine machine_;
		boost::shared_ptr<IInterestControl> interestControl_;

		PipelineControl(const PipelineControlStateMachine& machine,
			const boost::shared_ptr<IInterestControl>& interestControl);
	};
}

#endif