//
// segment-controller.hpp
//
//  Created by Peter Gusev on 04 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __segment_controller_h__
#define __segment_controller_h__

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>
#include <ndn-cpp/face.hpp>

#include "ndnrtc-object.hpp"
#include "name-components.hpp"
#include "periodic.hpp"
#include "statistics.hpp"

namespace ndn
{
class Interest;
class Data;
}

namespace ndnrtc
{
namespace statistics
{
class StatisticsStorage;
}

class WireSegment;
class ISegmentControllerObserver;
class SegmentControllerImpl;

class ISegmentController
{
  public:
    virtual unsigned int getCurrentIdleTime() const = 0;
    virtual unsigned int getMaxIdleTime() const = 0;
    virtual ndn::OnData getOnDataCallback() = 0;
    virtual ndn::OnTimeout getOnTimeoutCallback() = 0;
    virtual ndn::OnNetworkNack getOnNetworkNackCallback() = 0;
    virtual void attach(ISegmentControllerObserver *) = 0;
    virtual void detach(ISegmentControllerObserver *) = 0;
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
class SegmentController : public ISegmentController
{
    typedef statistics::StatisticsStorage StatStorage;
    typedef boost::shared_ptr<statistics::StatisticsStorage> StatStoragePtr;

  public:
    SegmentController(boost::asio::io_service &faceIo,
                      unsigned int maxIdleTimeMs,
                      StatStoragePtr storage =
                          StatStoragePtr(StatStorage::createConsumerStatistics()));

    unsigned int getCurrentIdleTime() const;
    unsigned int getMaxIdleTime() const;
    void setIsActive(bool active);
    bool getIsActive() const;

    ndn::OnData getOnDataCallback();
    ndn::OnTimeout getOnTimeoutCallback();
    ndn::OnNetworkNack getOnNetworkNackCallback();

    void attach(ISegmentControllerObserver *o);
    void detach(ISegmentControllerObserver *o);

    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

  private:
    boost::shared_ptr<SegmentControllerImpl> pimpl_;
};

class ISegmentControllerObserver
{
  public:
    /**
     * Called whenever new segment has arrived
     */
    virtual void segmentArrived(const boost::shared_ptr<WireSegment> &) = 0;

    /**
     * Called whenever interest has timed out
     */
    virtual void segmentRequestTimeout(const NamespaceInfo &, 
                                       const boost::shared_ptr<const ndn::Interest> &) = 0;

    /**
     * Called whenever interest gets network nack
     */
    virtual void segmentNack(const NamespaceInfo &, int,
                             const boost::shared_ptr<const ndn::Interest> &) = 0;

    /**
     * Called when no segments were received during specified time interval.
     * This doesn't fire repeatedly if data starvation continues.
     * @see getMaxIdleTime()
     */
    virtual void segmentStarvation() = 0;
};
}

#endif