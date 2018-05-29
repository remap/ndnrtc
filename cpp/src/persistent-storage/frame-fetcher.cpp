//
// frame-fetcher.cpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "frame-fetcher.hpp"
#include "persistent-storage/storage-engine.hpp"
#include "persistent-storage/fetching-task.hpp"
#include "frame-data.hpp"
#include "frame-buffer.hpp"
#include "video-decoder.hpp"

#include <ndn-cpp/name.hpp>

using namespace ndnrtc;
using namespace boost;
using namespace ndn;

FrameFetcher::FrameFetcher(const boost::shared_ptr<StorageEngine>& storage)
    : storage_(storage), 
      state_(Idle), 
      fetchSettings_({3,1000})
{
    fetchMethod_ = make_shared<FetchMethodLocal>(storage_);
    description_ = "frame-fetcher";
}

FrameFetcher::~FrameFetcher()
{
}

void
FrameFetcher::fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure)
{
    if (NameComponents::extractInfo(frameName, frameNameInfo_))
    {
        onBufferAllocate_ = onBufferAllocate;
        onFrameFetched_ = onFrameFetched;
        onFetchFailure_ = onFetchFailure;
        state_ = Fetching;

        if (!frameNameInfo_.isDelta_) // if it's a key frame - all is easy, just fetch it and decode
        {
            shared_ptr<FrameFetcher> self = shared_from_this();

            shared_ptr<FrameFetchingTask> task 
                = make_shared<FrameFetchingTask>(
                    frameName,
                    fetchMethod_,
                    [self, this](const boost::shared_ptr<const FetchingTask>& task, 
                       const boost::shared_ptr<const BufferSlot>& slot)
                    {
                        LogInfoC << "target frame fetched" << std::endl;
                        checkReadyDecode();
                    },
                    [self, this](const boost::shared_ptr<const FetchingTask>& task,
                      std::string reason)
                    {
                        LogErrorC << "failed to fetch target frame: " << reason << std::endl;
                        halt(reason);
                    },
                    fetchSettings_);

            LogInfoC << "initiating fetching for target frame " << frameName << std::endl;

            targetFrameTask_ = task;
            keyFrameTask_ = task;
            fetchingTasks_[frameName] = task;

            task->setLogger(getLogger());
            task->start();
        }
        else
        {
            // spawn fetching target frame
            shared_ptr<FrameFetcher> self = shared_from_this();
            shared_ptr<FrameFetchingTask> task 
                = make_shared<FrameFetchingTask>(
                    frameName,
                    fetchMethod_,
                    [self, this](const boost::shared_ptr<const FetchingTask>& task, 
                       const boost::shared_ptr<const BufferSlot>& slot)
                    {
                        LogInfoC << "target frame fetched" << std::endl;
                        checkReadyDecode();
                    },
                    [self, this](const boost::shared_ptr<const FetchingTask>& task,
                      std::string reason)
                    {
                        LogErrorC << "failed to fetch target frame: " << reason << std::endl;
                        halt(reason);
                    },
                    fetchSettings_,
                    [self, this](const boost::shared_ptr<const FetchingTask>& task,
                              const boost::shared_ptr<const SlotSegment>& segment)
                    {
                        // on first segment
                        // figure out Key frame # and request it here
                        fetchGopKey(segment);
                    });

            LogInfoC << "initiating fetching for target frame " << frameName << std::endl;

            targetFrameTask_ = task;
            fetchingTasks_[frameName] = task;

            task->setLogger(getLogger());
            task->start();
        }
    }
    else
        throw std::runtime_error("Bad frame name provided");
}

void 
FrameFetcher::fetchGopKey(const boost::shared_ptr<const SlotSegment>& deltaSegment)
{
    const shared_ptr<WireData<VideoFrameSegmentHeader>> videoFrameSegment = 
        dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(deltaSegment->getData());
    
    PacketNumber keyFrameNumber = videoFrameSegment->segment().getHeader().pairedSequenceNo_;
    Name keyFrameName = frameNameInfo_.getPrefix(prefix_filter::ThreadNT)
                                      .append(NameComponents::NameComponentKey)
                                      .appendSequenceNumber(keyFrameNumber);
    
    LogInfoC << "will fetch Key frame " << keyFrameNumber
             << " (" << keyFrameName << ")" << std::endl;
    
    shared_ptr<FrameFetcher> self = shared_from_this();
    shared_ptr<FrameFetchingTask> task = 
        make_shared<FrameFetchingTask>(
            keyFrameName,
            fetchMethod_,
            [self, this](const boost::shared_ptr<const FetchingTask>& task, 
                         const boost::shared_ptr<const BufferSlot>& slot)
            {
                LogInfoC << "key frame fetched" << std::endl;
                checkReadyDecode();
            },
            [self, this](const boost::shared_ptr<const FetchingTask>& task,
                         std::string reason)
            {
                LogErrorC << "failed to fetch Key frame: " << reason << std::endl;
                halt(reason);
            },
            fetchSettings_,
            [self, this](const boost::shared_ptr<const FetchingTask>& task,
                         const boost::shared_ptr<const SlotSegment>& segment)
            {
                // on first segment
                // figure out First Delta gop number and request them all
                fetchGopDelta(segment);
            },
            [](const boost::shared_ptr<const FetchingTask>& task,
                     const boost::shared_ptr<const SlotSegment>& segment)
            {
            });

    keyFrameTask_ = task;
    fetchingTasks_[keyFrameName] = task;

    task->setLogger(getLogger());
    task->start();
}

void
FrameFetcher::fetchGopDelta(const boost::shared_ptr<const SlotSegment>& segment)
{
    const shared_ptr<WireData<VideoFrameSegmentHeader>> videoFrameSegment = 
        dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(segment->getData());
    PacketNumber firstGopDeltaNumber = videoFrameSegment->segment().getHeader().pairedSequenceNo_;

    // check, how many delta frames we need to fetch
    int deltasToFetch = frameNameInfo_.sampleNo_ - firstGopDeltaNumber;

    if (deltasToFetch)
    {
        assert(deltasToFetch > 0);

        LogInfoC << "will fetch " << deltasToFetch << " delta frames " << std::endl;

        Name prefix(frameNameInfo_.getPrefix(prefix_filter::Thread));

        for (PacketNumber deltaSeqNo = firstGopDeltaNumber; 
             deltaSeqNo < frameNameInfo_.sampleNo_; 
             ++deltaSeqNo)
        {
            Name deltaFrameName(prefix);
            deltaFrameName.appendSequenceNumber(deltaSeqNo);

            LogDebugC << "will fetch " << deltaFrameName << std::endl;

            shared_ptr<FrameFetcher> self = shared_from_this();
            shared_ptr<FrameFetchingTask> task = 
                make_shared<FrameFetchingTask>(
                    deltaFrameName,
                    fetchMethod_,
                    [self, this, deltaSeqNo](const boost::shared_ptr<const FetchingTask>& task, 
                                const boost::shared_ptr<const BufferSlot>& slot)
                    {
                        LogDebugC << "delta " << deltaSeqNo << " fetched" << std::endl;
                        checkReadyDecode();
                    },
                    [self, this](const boost::shared_ptr<const FetchingTask>& task,
                                 std::string reason)
                    {
                        LogErrorC << "failed to fetch delta frame: " << reason << std::endl;
                        halt(reason);
                    },
                    fetchSettings_);

            deltasTasks_[deltaFrameName] = task;
            fetchingTasks_[deltaFrameName] = task;

            task->setLogger(getLogger());
            task->start();
        }
    }
}

void
FrameFetcher::checkReadyDecode()
{
    if (state_ == Fetching)
    {
        // LogInfoC << "checking " << fetchingTasks_.size() << " tasks..." << std::endl;

        // check if we have all that's needed to decoding...
        bool allFetched = true;
        for (auto t:fetchingTasks_)
            allFetched &= (t.second->getState() == FrameFetchingTask::Completed);

        if (allFetched)
        {
            state_ = Decoding;
            decode();
        }
    }
}

void
FrameFetcher::decode()
{
    // initialize decoder here and start decoding...
    VideoFrameSlot frameSlot;
    bool recovered = false;
    shared_ptr<const BufferSlot> slot = keyFrameTask_->getSlot();
    shared_ptr<ImmutableVideoFramePacket> framePacket =
        frameSlot.readPacket(*slot, recovered);
    
    if (framePacket.get())
    {
        VideoFrameSegmentHeader header = frameSlot.readSegmentHeader(*slot);
        FrameInfo finfo({ (uint64_t)(slot->getHeader().publishUnixTimestamp_*1000),
                                     header.playbackNo_,
                                     slot->getPrefix().toUri() });
        VideoCoderParams params = setupDecoderParams(framePacket);
        shared_ptr<FrameFetcher> self = shared_from_this();
        int needToDecode = fetchingTasks_.size();
        OnDecodedImage onDecoded = 
            [&needToDecode, self, this](const FrameInfo& fi, const WebRtcVideoFrame& f){
                --needToDecode;

                if (needToDecode == 0)
                {
                    uint8_t* buffer = onBufferAllocate_(self, f.width(), f.height());
                    if (buffer)
                    {
                        state_ = Completed;

                        ConvertFromI420(f, webrtc::kBGRA, 0, buffer);
                        onFrameFetched_(self, fi, fetchingTasks_.size(), 
                                        f.width(), f.height(), buffer);
                    }
                    else
                        LogWarnC << "received null buffer for frame" << std::endl;
                    reset();
                }
            };
        boost::shared_ptr<VideoDecoder> decoder = 
            boost::make_shared<VideoDecoder>(params, onDecoded);

        LogDebugC << "need to decode " << needToDecode << " frames. decoding Key" << std::endl;
        assert(needToDecode > 0);
        decoder->processFrame(finfo, framePacket->getFrame());

        // decode deltas if needed
        for (auto t:deltasTasks_)
        {
            LogDebugC << "decoding Delta " << needToDecode << std::endl;

            slot = t.second->getSlot();
            framePacket = frameSlot.readPacket(*slot, recovered);
            if (framePacket.get())
                decoder->processFrame(finfo, framePacket->getFrame());
            else
                halt("Couldn't retrieve frame from "+t.second->getSlot()->getPrefix().toUri());
        }

        if (targetFrameTask_ != keyFrameTask_)
        {
            assert(needToDecode == 1);
            LogDebugC << "decoding target frame " << needToDecode << std::endl;

            slot = targetFrameTask_->getSlot();
            framePacket = frameSlot.readPacket(*slot, recovered);
            if (framePacket.get())
            {
                header = frameSlot.readSegmentHeader(*slot);
                finfo.timestamp_ = (uint64_t)(slot->getHeader().publishUnixTimestamp_*1000);
                finfo.playbackNo_ = header.playbackNo_;
                finfo.ndnName_ = slot->getPrefix().toUri();

                decoder->processFrame(finfo, framePacket->getFrame());
            }
            else
                halt("Couldn't retrieve frame from "+targetFrameTask_->getSlot()->getPrefix().toUri());
        }

        assert(needToDecode == 0);
    }
    else
        halt("Couldn't retrieve frame from "+keyFrameTask_->getSlot()->getPrefix().toUri());
}

void
FrameFetcher::reset()
{
    for (auto t:fetchingTasks_)
    {
        t.second->cancel();
        t.second.reset();
    }

    fetchingTasks_.clear();
    deltasTasks_.clear();
    keyFrameTask_.reset();
    targetFrameTask_.reset();
}

void
FrameFetcher::halt(std::string reason)
{
    if (state_ != Failed)
    {
        state_ = Failed;
        onFetchFailure_(shared_from_this(), reason);
        reset();
    }
}

VideoCoderParams
FrameFetcher::setupDecoderParams(const boost::shared_ptr<ImmutableVideoFramePacket>& fp) const
{
    VideoCoderParams p;
    p.codecFrameRate_ = 30;
    p.gop_ = 30;
    p.startBitrate_ = 1000;
    p.maxBitrate_ = 1000;
    p.encodeHeight_ = fp->getFrame()._encodedHeight;
    p.encodeWidth_ = fp->getFrame()._encodedWidth;
    p.dropFramesOn_ = false;

    return p;
}