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

	class ISegmentController {
	public:
		virtual unsigned int getCurrentIdleTime() const = 0;
		virtual unsigned int getMaxIdleTime() const = 0;
		virtual ndn::OnData getOnDataCallback() = 0;
		virtual ndn::OnTimeout getOnTimeoutCallback() = 0;
		virtual void attach(ISegmentControllerObserver*) = 0;
		virtual void detach(ISegmentControllerObserver*) = 0;
	};

	/**
	 * SegmentController is the first responder for incoming data. It combines
	 * incoming segment and Interest that requested it in a WireSegment structure
	 * which is passed further to any attached observer. SegmentController provides
	 * OnData and OnTimeout callbacks that should be used for Interests expression.
	 * SegmentController also checks for incoming data flow interruptions - it will
	 * notify all attached observers if data has not arrived during specified period 
	 * of time.
	 */
	class SegmentController : public NdnRtcComponent,
							  public ISegmentController, 
							  private Periodic
	{
	public:
		SegmentController(boost::asio::io_service& faceIo, 
			unsigned int maxIdleTimeMs);
		~SegmentController();

		unsigned int getCurrentIdleTime() const;
		unsigned int getMaxIdleTime() const { return maxIdleTimeMs_; }
        void setIsActive(bool active);
        bool getIsActive() const { return active_; }

		ndn::OnData getOnDataCallback();
		ndn::OnTimeout getOnTimeoutCallback();

		void attach(ISegmentControllerObserver* o);
		void detach(ISegmentControllerObserver* o);

	private:
        bool active_;
		boost::mutex mutex_;
		unsigned int maxIdleTimeMs_;
		std::vector<ISegmentControllerObserver*> observers_;
		int64_t lastDataTimestampMs_;
		bool starvationFired_;

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