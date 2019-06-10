//
// playout-control.hpp
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __playout_control_h__
#define __playout_control_h__

#include "ndnrtc-object.hpp"
#include "frame-buffer.hpp"
#include "playout-impl.hpp"
#include "drd-estimator.hpp"
#include "estimators.hpp"

namespace ndnrtc
{
class IPlayout;
class IPlaybackQueue;
class RetransmissionController;

class IPlayoutControl : public IPlaybackQueueObserver,
                        public IPlayoutObserver,
                        public IDrdEstimatorObserver
{
  public:
    virtual void allowPlayout(bool allow, int ffwdMs = 0) = 0;
    virtual void onNewSampleReady() = 0;
    virtual void onQueueEmpty() = 0;
    virtual void setThreshold(unsigned int t) = 0;
    virtual unsigned int getThreshold() const = 0;
};

/**
 * PlayoutControl implements functionality to start/stop samples playout.
 * Playout starts whenever it is allowed AND playback queue size reached
 * or excedes minimal playable level (target size). It will fast forward
 * the excess upon start.
 * Playout stops if is is not allowed.
 */
class PlayoutControl : public NdnRtcComponent,
                       public IPlayoutControl
{
  public:
    PlayoutControl(const boost::shared_ptr<IPlayout> &playout,
                   const boost::shared_ptr<IPlaybackQueue> &queue,
                   const boost::shared_ptr<RetransmissionController> &rtxController,
                   unsigned int minimalPlayableLevel = PlayoutControl::MinimalPlayableLevel);

    void allowPlayout(bool allow, int ffwdMs = 0) override;
    void onNewSampleReady() override;
    void onQueueEmpty() override { /*ignored*/ }
    // void onSamplePlayed() { /*ignored*/ }
    void setThreshold(unsigned int t) override;
    unsigned int getThreshold() const override { return thresholdMs_; }

    const boost::shared_ptr<const IPlayout> getPlayoutMechanism() const { return playout_; }
    const boost::shared_ptr<const IPlaybackQueue> getPlaybackQueue() const { return queue_; }

    static unsigned int MinimalPlayableLevel;
  private:
    bool playoutAllowed_;
    int ffwdMs_;
    int64_t queueCheckMs_;
    estimators::Average playbackQueueSize_;
    boost::shared_ptr<IPlayout> playout_;
    boost::shared_ptr<IPlaybackQueue> queue_;
    boost::shared_ptr<RetransmissionController> rtxController_;
    unsigned int userThresholdMs_, thresholdMs_;

    void onDrdUpdate() override {}
    void onCachedDrdUpdate(double, double) override{}
    void onOriginalDrdUpdate(double, double) override;

    void checkPlayout();
    void checkPlaybackQueueSize();
};
}

#endif
