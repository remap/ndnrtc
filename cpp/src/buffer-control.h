// 
// buffer-control.h
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_control_h__
#define __buffer_control_h__

#include "segment-controller.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"

namespace ndnrtc {
	class DrdEstimator;
	class Buffer;
	class IBufferLatencyControl;

	/**
	 * Buffer Control class performs adding incoming segments to frame
	 * buffer and updates several parameters:
	 *  - DRD estimation
	 *  - Target buffer size
	 * @see DrdEstimator, Buffer
	 */
	class BufferControl : public ISegmentControllerObserver, public NdnRtcComponent
	{
	public:
		BufferControl(const boost::shared_ptr<DrdEstimator>&, const boost::shared_ptr<Buffer>&,
			const boost::shared_ptr<IBufferLatencyControl>&);
		~BufferControl();

		void segmentArrived(const boost::shared_ptr<WireSegment>&);

		void segmentRequestTimeout(const NamespaceInfo&){ /*ignored*/ }
		void segmentStarvation(){ /*ignored*/ }
		
	private:
		boost::shared_ptr<DrdEstimator> drdEstimator_;
		boost::shared_ptr<Buffer> buffer_;
		boost::shared_ptr<IBufferLatencyControl> latencyControl_;

		void informLatencyControl(const Buffer::Receipt&);
	};

	class IBufferLatencyControl {
	public:
		virtual void setTargetRate(double rate) = 0;
		virtual void sampleArrived() = 0;
	};
}

#endif