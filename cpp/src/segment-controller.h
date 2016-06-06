// 
// segment-controller.h
//
//  Created by Peter Gusev on 04 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __segment_controller_h__
#define __segment_controller_h__

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <ndn-cpp/face.hpp>

#include "ndnrtc-object.h"
#include "name-components.h"
#include "periodic.h"

namespace ndn {
	class Interest;
	class Data;
}

namespace ndnrtc {
	class WireSegment;
	class ISegmentControllerObserver;

	class SegmentController : public NdnRtcComponent, private Periodic {
	public:
		SegmentController(boost::asio::io_service& faceIo, 
			unsigned int maxIdleTimeMs);
		~SegmentController();

		unsigned int getCurrentIdleTime() const;
		unsigned int getMaxIdleTime() const { return maxIdleTimeMs_; }

		ndn::OnData getOnDataCallback();
		ndn::OnTimeout getOnTimeoutCallback();

		void attach(ISegmentControllerObserver* o);
		void detach(ISegmentControllerObserver* o);

	private:
		boost::mutex mutex_;
		unsigned int maxIdleTimeMs_;
		std::vector<ISegmentControllerObserver*> observers_;
		int64_t lastDataTimestampMs_;
		bool starvationFired_;

		void setupIdleTimer(unsigned int timeMs);
		void cancelIdleTimer();
		void checkStarvation();
		unsigned int periodicInvocation();

		void onData(const boost::shared_ptr<const ndn::Interest>&,
			const boost::shared_ptr<ndn::Data>&);
		void onTimeout(const boost::shared_ptr<const ndn::Interest>&);
	};

	class ISegmentControllerObserver {
	public:
		/**
		 * Called whenever new segment has arrived
		 */
		virtual void segmentArrived(const boost::shared_ptr<WireSegment>&) = 0;

		/**
		 * Called whenever interest has timed out
		 */
		virtual void segmentRequestTimeout(const NamespaceInfo&) = 0;

		/**
		 * Called when no segments were received during specified time interval.
		 * This doesn't fire repeatedly if data starvation continues.
		 * @see getMaxIdleTime()
		 */
		virtual void segmentStarvation() = 0;
	};
}

#endif