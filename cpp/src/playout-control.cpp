//
// playout-control.cpp
//
//  Created by Peter Gusev on 16 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "playout-control.hpp"

#include "playout.hpp"
#include "frame-buffer.hpp"

using namespace ndnrtc;

PlayoutControl::PlayoutControl(const boost::shared_ptr<IPlayout> &playout,
                               const boost::shared_ptr<IPlaybackQueue> queue,
                               unsigned int minimalPlayableLevel)
    : playoutAllowed_(false),
      ffwdMs_(0),
      playout_(playout),
      queue_(queue),
      thresholdMs_(minimalPlayableLevel)
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
            }
        }
        else
        {
            LogInfoC << "stopping playout" << std::endl;
            playout_->stop();
        }
    } // if isRunning != playoutAllowed
}
