// 
// video-playout-impl.cpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "video-playout-impl.h"
#include "frame-data.h"
#include "frame-buffer.h"
#include "statistics.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndnlog;

//******************************************************************************
VideoPlayoutImpl::VideoPlayoutImpl(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatisticsStorage>& statStorage):
PlayoutImpl(io, queue, statStorage),
gopIsValid_(false), currentPlayNo_(-1), 
gopCount_(0), frameConsumer_(nullptr)
{
    setDescription("vplayout");
}

void VideoPlayoutImpl::registerFrameConsumer(IEncodedFrameConsumer* frameConsumer)
{ 
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    frameConsumer_ = frameConsumer; 
}

void VideoPlayoutImpl::deregisterFrameConsumer()
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    frameConsumer_ = nullptr;
}

void VideoPlayoutImpl::attach(IVideoPlayoutObserver* observer)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    PlayoutImpl::attach(observer);
}

void VideoPlayoutImpl::detach(IVideoPlayoutObserver* observer)
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
    PlayoutImpl::detach(observer);
}

void VideoPlayoutImpl::stop()
{
    PlayoutImpl::stop();
    currentPlayNo_ = -1;
    gopCount_ = 0;
}

//******************************************************************************
void VideoPlayoutImpl::processSample(const boost::shared_ptr<const BufferSlot>& slot)
{
    LogTraceC << "processing sample " << slot->dump() << std::endl;

    bool recovered = false;
    boost::shared_ptr<ImmutableVideoFramePacket> framePacket =
        frameSlot_.readPacket(*slot, recovered);

    if (framePacket.get())
    {
        VideoFrameSegmentHeader hdr = frameSlot_.readSegmentHeader(*slot);

        if (recovered)
        {
            LogDebugC << "recovered " << slot->getNameInfo().sampleNo_
                << " (" << hdr.playbackNo_ << ")" << std::endl;
            
            (*statStorage_)[Indicator::RecoveredNum]++;
            if (slot->getNameInfo().class_ == SampleClass::Key)
                (*statStorage_)[Indicator::RecoveredKeyNum]++;
        }

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
            
            (*statStorage_)[Indicator::SkippedNum]++;
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
            
            (*statStorage_)[Indicator::PlayedNum]++;
            (*statStorage_)[Indicator::LastPlayedNo] = currentPlayNo_;
            if (slot->getNameInfo().class_ == SampleClass::Key)
            {
                (*statStorage_)[Indicator::LastPlayedKeyNo] = slot->getNameInfo().sampleNo_;
                (*statStorage_)[Indicator::PlayedKeyNum]++;
            }
            else
                (*statStorage_)[Indicator::LastPlayedDeltaNo] = slot->getNameInfo().sampleNo_;
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
