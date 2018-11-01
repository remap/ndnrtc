// 
// video-playout-impl.cpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "video-playout-impl.hpp"
#include "frame-data.hpp"
#include "frame-buffer.hpp"
#include "statistics.hpp"

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
bool VideoPlayoutImpl::processSample(const boost::shared_ptr<const BufferSlot>& slot)
{
    LogTraceC << "processing sample " << slot->dump() << std::endl;

    bool recovered = false;
    boost::shared_ptr<ImmutableVideoFramePacket> framePacket =
        frameSlot_.readPacket(*slot, recovered);

    if (framePacket.get())
    {
        VideoFrameSegmentHeader hdr = frameSlot_.readSegmentHeader(*slot);
        stringstream ss;
        ss << slot->getNameInfo().sampleNo_
           << (slot->getNameInfo().isDelta_ ? "d/" : "k/")
           << hdr.playbackNo_ << "p";
        string frameStr = ss.str();

        if (recovered)
        {
            LogDebugC << "recovered " << frameStr << std::endl;
            
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
        else
        {
            if (currentPlayNo_ >= 0 &&
                (hdr.playbackNo_ != currentPlayNo_ + 1 || !gopIsValid_))
            {
                if (!gopIsValid_)
                    LogWarnC << "skip " << frameStr << ". invalid GOP" << std::endl;
                else
                    LogWarnC << "skip " << frameStr
                             << " (expected " << currentPlayNo_ + 1 << "p)"
                             << std::endl;

                gopIsValid_ = false;

                {
                    boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
                    for (auto o : observers_)
                        ((IVideoPlayoutObserver *)o)->frameSkipped(hdr.playbackNo_, !slot->getNameInfo().isDelta_);
                }

                (*statStorage_)[Indicator::SkippedNum]++;
            }
        }

        currentPlayNo_ = hdr.playbackNo_;

        if (gopIsValid_)
        {
            {
                boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
                if (frameConsumer_)
                {
                    if (framePacket->isValid())
                    {
                        FrameInfo finfo({ (uint64_t)(slot->getHeader().publishUnixTimestamp_*1000), 
                                          currentPlayNo_, 
                                          slot->getPrefix().toUri(),
                                          !slot->getNameInfo().isDelta_});
                        frameConsumer_->processFrame(finfo, framePacket->getFrame());
                    }
                    else
                    {
                        LogErrorC << "frame packet is not valid " << std::endl;
                        assert(false);
                    }
                }
            
                for (auto o:observers_) 
                    ((IVideoPlayoutObserver*)o)->frameProcessed(hdr.playbackNo_, 
                        !slot->getNameInfo().isDelta_);
            }

            LogTraceC << "processed " << frameStr << std::endl;
            
            (*statStorage_)[Indicator::PlayedNum]++;
            (*statStorage_)[Indicator::LastPlayedNo] = currentPlayNo_;
            if (slot->getNameInfo().class_ == SampleClass::Key)
            {
                (*statStorage_)[Indicator::LastPlayedKeyNo] = slot->getNameInfo().sampleNo_;
                (*statStorage_)[Indicator::PlayedKeyNum]++;
            }
            else
                (*statStorage_)[Indicator::LastPlayedDeltaNo] = slot->getNameInfo().sampleNo_;
        } // gop is valid

        return gopIsValid_;
    }
    else
    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(mutex_);
        LogWarnC << "failed recovery " << slot->dump() << std::endl;

        for (auto o:observers_) 
            ((IVideoPlayoutObserver*)o)->recoveryFailure(slot->getNameInfo().sampleNo_,
                    !slot->getNameInfo().isDelta_);
    }

    return false;
}
