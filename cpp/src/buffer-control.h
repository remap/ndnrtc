// 
// buffer-control.h
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_control_h__
#define __buffer_control_h__

#include "ndnrtc-common.h"
#include "segment-controller.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"

namespace ndnrtc {
    namespace statistics {
        class StatisticsStorage;
    }
    
	class DrdEstimator;
	class Buffer;
	class IBufferControlObserver;

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
		BufferControl(const boost::shared_ptr<DrdEstimator>&,
                      const boost::shared_ptr<IBuffer>&,
                      const boost::shared_ptr<statistics::StatisticsStorage>& storage);

		void attach(IBufferControlObserver*);
		void detach(IBufferControlObserver*);

		void segmentArrived(const boost::shared_ptr<WireSegment>&);
		void segmentRequestTimeout(const NamespaceInfo&){ /*ignored*/ }
		void segmentStarvation(){ /*ignored*/ }
		
	private:
		std::vector<IBufferControlObserver*> observers_;
		boost::shared_ptr<DrdEstimator> drdEstimator_;
		boost::shared_ptr<IBuffer> buffer_;
        boost::shared_ptr<statistics::StatisticsStorage> sstorage_;

		void informLatencyControl(const BufferReceipt&);
	};

	class IBufferControlObserver {
	public:
		/**
		 * Called whenever sample rate value is retrieved from sample packet metadata
		 * @param rate Sample rate
		 */
		virtual void targetRateUpdate(double rate) = 0;
		/**
		 * Called whenever new sample arrived (not, segment!)
		 * @param playbackNo Sample playback number
		 */
		virtual void sampleArrived(const PacketNumber& playbackNo) = 0;
	};
}

#endif