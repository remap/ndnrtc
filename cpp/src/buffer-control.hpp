//
// buffer-control.hpp
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_control_h__
#define __buffer_control_h__

#include <boost/shared_ptr.hpp>

#include "ndnrtc-common.hpp"
#include "segment-controller.hpp"
#include "ndnrtc-object.hpp"
#include "frame-buffer.hpp"

namespace ndn {
    class Interest;
}

namespace ndnrtc
{
namespace statistics
{
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
    BufferControl(const boost::shared_ptr<DrdEstimator> &,
                  const boost::shared_ptr<IBuffer> &,
                  const boost::shared_ptr<statistics::StatisticsStorage> &storage);

    void attach(IBufferControlObserver *);
    void detach(IBufferControlObserver *);

    void segmentArrived(const boost::shared_ptr<WireSegment> &);
    void segmentRequestTimeout(const NamespaceInfo &, 
                               const boost::shared_ptr<const ndn::Interest> &) { /*ignored*/}
    void segmentNack(const NamespaceInfo &, int, 
                     const boost::shared_ptr<const ndn::Interest> &) { /*ignored*/}
    void segmentStarvation() { /*ignored*/}

  private:
    std::vector<IBufferControlObserver *> observers_;
    boost::shared_ptr<DrdEstimator> drdEstimator_;
    boost::shared_ptr<IBuffer> buffer_;
    boost::shared_ptr<statistics::StatisticsStorage> sstorage_;
};

class IBufferControlObserver
{
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
    virtual void sampleArrived(const PacketNumber &playbackNo) = 0;
};
}

#endif
