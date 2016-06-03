//
//  video-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include "video-playout.h"
#include "frame-data.h"
#include "frame-buffer.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnlog;

//******************************************************************************
VideoPlayout::VideoPlayout(boost::asio::io_service& io,
            const boost::shared_ptr<PlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage>& statStorage):
Playout(io, queue, statStorage),
gopIsValid_(false), currentPlayNo_(-1), 
gopCount_(0), frameConsumer_(nullptr)
{
    setDescription("vplayout");
}

void VideoPlayout::registerFrameConsumer(IEncodedFrameConsumer* frameConsumer)
{ 
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    frameConsumer_ = frameConsumer; 
}

void VideoPlayout::deregisterFrameConsumer()
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    frameConsumer_ = nullptr;
}

void VideoPlayout::attach(IVideoPlayoutObserver* observer)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    Playout::attach(observer);
}

void VideoPlayout::detach(IVideoPlayoutObserver* observer)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    Playout::detach(observer);
}

void VideoPlayout::stop()
{
    Playout::stop();
    currentPlayNo_ = -1;
    gopCount_ = 0;
}

//******************************************************************************
void VideoPlayout::processSample(const boost::shared_ptr<const BufferSlot>& slot)
{
    LogTraceC << "processing sample " << slot->dump() << std::endl;

    bool recovered = false;
    boost::shared_ptr<ImmutableVideoFramePacket> framePacket =
        frameSlot_.readPacket(*slot, recovered);

    if (framePacket.get())
    {
        VideoFrameSegmentHeader hdr = frameSlot_.readSegmentHeader(*slot);

        if (recovered)
            LogDebugC << "recovered " << slot->getNameInfo().sampleNo_ 
                << " (" << hdr.playbackNo_ << ")" << std::endl;

        if (!slot->getNameInfo().isDelta_)
        {
            gopIsValid_ = true; 
            ++gopCount_;

            LogTraceC << "gop " << gopCount_ << std::endl;
        }

        if (currentPlayNo_ >= 0 &&
            (hdr.playbackNo_ != currentPlayNo_+1 || !gopIsValid_))
        {
            if (!gopIsValid_)
                LogWarnC << "skip " << slot->getNameInfo().sampleNo_ 
                    << " invalid GOP" << std::endl;
            else
                LogWarnC << "skip " << slot->getNameInfo().sampleNo_ 
                    << " (expected " << currentPlayNo_+1 
                    << " received " << hdr.playbackNo_ << ")" << std::endl;

            gopIsValid_ = false;

            boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
            for (auto o:observers_) 
                ((IVideoPlayoutObserver*)o)->frameSkipped(hdr.playbackNo_, 
                    !slot->getNameInfo().isDelta_);
        }

        currentPlayNo_ = hdr.playbackNo_;

        if (gopIsValid_)
        {
            boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
            if (frameConsumer_)
                frameConsumer_->processFrame(framePacket);
            
            for (auto o:observers_) 
                ((IVideoPlayoutObserver*)o)->frameProcessed(hdr.playbackNo_, 
                    !slot->getNameInfo().isDelta_);

            LogDebugC << "processed " << slot->getNameInfo().sampleNo_
                << " (" << hdr.playbackNo_ << ")" << std::endl;
        }
    }
    else
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        LogWarnC << "failed recovery " << slot->dump() << std::endl;

        for (auto o:observers_) 
                ((IVideoPlayoutObserver*)o)->recoveryFailure(slot->getNameInfo().sampleNo_, 
                    !slot->getNameInfo().isDelta_);
    }
}
