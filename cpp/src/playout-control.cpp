//
// playout-control.cpp
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "playout-control.hpp"

#include "playout.hpp"
#include "frame-buffer.hpp"
#include "rtx-controller.hpp"
#include "clock.hpp"

using namespace ndnrtc;
using namespace estimators;

#define ALPHA 2
#define QUEUE_CHECK_INTERVAL 300 // milliseconds
#define QUEUE_DIFF_THRESHOLD 1.5 // frames
#define DRD_DIFF_THRESHOLD 0.1 // percent

// minimal default size of the playback queue
unsigned int PlayoutControl::MinimalPlayableLevel = 150;

PlayoutControl::PlayoutControl(const boost::shared_ptr<IPlayout> &playout,
                               const boost::shared_ptr<IPlaybackQueue> &queue,
                               const boost::shared_ptr<RetransmissionController> &rtxController,
                               unsigned int minimalPlayableLevel)
    : playoutAllowed_(false),
      ffwdMs_(0), queueCheckMs_(0),
      playbackQueueSize_(Average(boost::make_shared<TimeWindow>(QUEUE_CHECK_INTERVAL))),
      playout_(playout),
      queue_(queue),
      rtxController_(rtxController),
      userThresholdMs_(minimalPlayableLevel),
      thresholdMs_(fmax(minimalPlayableLevel, PlayoutControl::MinimalPlayableLevel))
{
    description_ = "playout-control";
}

void PlayoutControl::allowPlayout(bool allow, int ffwdMs)
{
    if (allow)
        LogDebugC << "playout allowed: " << allow
            << " fast forward: " << ffwdMs << "ms"  << std::endl;

    playoutAllowed_ = allow;
    ffwdMs_ = ffwdMs;
    checkPlayout();
}

void PlayoutControl::onNewSampleReady()
{
    checkPlayout();

    if (playout_->isRunning())
    {
        playbackQueueSize_.newValue((double)queue_->size());
        checkPlaybackQueueSize();
    }
}

void PlayoutControl::setThreshold(unsigned int t)
{
    userThresholdMs_ = t;
    if (thresholdMs_ > userThresholdMs_)
        thresholdMs_ = fmax(userThresholdMs_, PlayoutControl::MinimalPlayableLevel);
    else
        thresholdMs_ = userThresholdMs_;
}

void PlayoutControl::checkPlayout()
{
    if (playout_->isRunning() ^ playoutAllowed_)
    {
        if (playoutAllowed_)
        {
            unsigned pqsize = queue_->size();
            if (pqsize >= (thresholdMs_+ffwdMs_))
            {
                LogInfoC << "playback queue size (" << pqsize
                         << ") passed target + ffwd size (" << thresholdMs_+ffwdMs_
                         << "). starting playout" << std::endl;

                playout_->start(pqsize - thresholdMs_);
                rtxController_->setEnabled(true);

                // enable queue checking a bit later - give some time for
                // queue to stabilize (after fastforward)
                queueCheckMs_ = clock::millisecondTimestamp() + QUEUE_CHECK_INTERVAL;
            }
        }
        else
        {
            LogInfoC << "stopping playout" << std::endl;
            playout_->stop();
            rtxController_->setEnabled(false);
        }
    } // if isRunning != playoutAllowed
}

void PlayoutControl::checkPlaybackQueueSize()
{
    int64_t now = clock::millisecondTimestamp();

    // check queue size
    if (now - queueCheckMs_ >= QUEUE_CHECK_INTERVAL)
    {
        queueCheckMs_ = now;
        double diffMs = (double)(thresholdMs_ - (double)queue_->size()); //playbackQueueSize_.value());
        double framesDiff = fabs(diffMs / queue_->samplePeriod());
        int dir = (diffMs < 0 ? -1: 1);

        if (framesDiff > QUEUE_DIFF_THRESHOLD)
        {
            double maxAdjustment = QUEUE_DIFF_THRESHOLD*queue_->samplePeriod();
            int64_t adjustmentMs = dir * (int64_t)fmin(fabs(diffMs), maxAdjustment);

            LogDebugC << "adjusting playback queue by " << adjustmentMs << "ms" << std::endl;
            playout_->addAdjustment(adjustmentMs);
        }
    }
}

void PlayoutControl::onOriginalDrdUpdate(double drd, double dev)
{
    double target = drd + ALPHA*dev;
    double diff = fabs((double)thresholdMs_-target)/target;

    if (diff >= DRD_DIFF_THRESHOLD  &&
        target >= PlayoutControl::MinimalPlayableLevel &&
        thresholdMs_ != userThresholdMs_)
    {
        LogTraceC << "target playback queue size " << thresholdMs_
                  << " differs from DRD " << drd
                  << " (by " << diff << ")" << std::endl;

        thresholdMs_ = target;

        LogDebugC << "target playback queue size set to " << thresholdMs_ << std::endl;
    }
}
