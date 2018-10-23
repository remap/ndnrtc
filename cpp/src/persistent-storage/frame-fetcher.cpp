//
// frame-fetcher.cpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "frame-fetcher.hpp"
#include "storage-engine.hpp"
#include "persistent-storage/fetching-task.hpp"
#include "frame-data.hpp"
#include "frame-buffer.hpp"
#include "video-decoder.hpp"

#include <ndn-cpp/name.hpp>

using namespace ndnrtc;
using namespace boost;
using namespace ndn;

namespace ndnrtc {
    class FrameFetcherImpl : public IFrameFetcher,
                             public ndnlog::new_api::ILoggingObject,
                             public boost::enable_shared_from_this<FrameFetcherImpl> {
    public:
        FrameFetcherImpl(const boost::shared_ptr<StorageEngine>& storage);
        FrameFetcherImpl(const boost::shared_ptr<Face>& face, const boost::shared_ptr<KeyChain>& keyChain);
        ~FrameFetcherImpl(){ reset(); }

        void fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure);
        const ndn::Name& getName() const
        {
            return frameNameInfo_.getPrefix(prefix_filter::Sample);
        }

        FrameFetcher::State getState() const { return state_; }

    private:
        FrameFetcher::State state_;
        FetchingTask::Settings fetchSettings_;

        boost::shared_ptr<StorageEngine> storage_;
        NamespaceInfo frameNameInfo_;
        OnBufferAllocate onBufferAllocate_;
        OnFrameFetched onFrameFetched_;
        OnFetchFailure onFetchFailure_;

        boost::shared_ptr<IFetchMethod> fetchMethod_;
        std::map<ndn::Name, boost::shared_ptr<FrameFetchingTask>> fetchingTasks_;
        boost::shared_ptr<FrameFetchingTask> keyFrameTask_, targetFrameTask_;
        std::map<ndn::Name, boost::shared_ptr<FrameFetchingTask>> deltasTasks_;

        void fetchGopKey(const boost::shared_ptr<const SlotSegment>& deltaSegment);
        void fetchGopDelta(const boost::shared_ptr<const SlotSegment>& segment);
        void checkReadyDecode();
        void decode();
        void reset();
        void halt(std::string reason);
        VideoCoderParams setupDecoderParams(const boost::shared_ptr<ImmutableVideoFramePacket>&) const;
    };
}

//******************************************************************************
FrameFetcher::FrameFetcher(const boost::shared_ptr<StorageEngine>& storage):
    pimpl_(make_shared<FrameFetcherImpl>(storage)){}
FrameFetcher::FrameFetcher(const boost::shared_ptr<Face>& face, const boost::shared_ptr<KeyChain>& keyChain):
    pimpl_(make_shared<FrameFetcherImpl>(face, keyChain)){}

void
FrameFetcher::fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure)
{
    pimpl_->fetch(frameName, onBufferAllocate, onFrameFetched, onFetchFailure);
}

const ndn::Name&
FrameFetcher::getName() const
{
    return pimpl_->getName();
}

FrameFetcher::State
FrameFetcher::getState() const 
{ 
    return pimpl_->getState(); 
}

void
FrameFetcher::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger)
{ 
    pimpl_->setLogger(logger); 
}

//******************************************************************************
FrameFetcherImpl::FrameFetcherImpl(const boost::shared_ptr<StorageEngine>& storage)
    : storage_(storage), 
      state_(FrameFetcher::Idle), 
      fetchSettings_({3,1000})
{
    fetchMethod_ = make_shared<FetchMethodLocal>(storage_);
    description_ = "frame-fetcher";
}

FrameFetcherImpl::FrameFetcherImpl(const boost::shared_ptr<Face>& face, const boost::shared_ptr<KeyChain>& keyChain)
    : state_(FrameFetcher::Idle), 
      fetchSettings_({3,1000})
{
    fetchMethod_ = make_shared<FetchMethodRemote>(face);
    description_ = "frame-fetcher";
}

void
FrameFetcherImpl::fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure)
{
    if (NameComponents::extractInfo(frameName, frameNameInfo_))
    {
        // TODO: if fetching is asynchronous and multiple fetch calls invoked in
        // rapid succession, these callbacks will be overwritten with the callbacks
        // from the latest invocation.
        // Needs to be fixed by storing callbacks per invocation.
        onBufferAllocate_ = onBufferAllocate;
        onFrameFetched_ = onFrameFetched;
        onFetchFailure_ = onFetchFailure;
        state_ = FrameFetcher::Fetching;

        if (!frameNameInfo_.isDelta_) // if it's a key frame - all is easy, just fetch it and decode
        {
            shared_ptr<FrameFetcherImpl> self = shared_from_this();
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
            shared_ptr<FrameFetcherImpl> self = shared_from_this();
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
FrameFetcherImpl::fetchGopKey(const boost::shared_ptr<const SlotSegment>& deltaSegment)
{
    const shared_ptr<WireData<VideoFrameSegmentHeader>> videoFrameSegment = 
        dynamic_pointer_cast<WireData<VideoFrameSegmentHeader>>(deltaSegment->getData());
    
    PacketNumber keyFrameNumber = videoFrameSegment->segment().getHeader().pairedSequenceNo_;
    Name keyFrameName = frameNameInfo_.getPrefix(prefix_filter::ThreadNT)
                                      .append(NameComponents::NameComponentKey)
                                      .appendSequenceNumber(keyFrameNumber);
    
    LogInfoC << "will fetch Key frame " << keyFrameNumber
             << " (" << keyFrameName << ")" << std::endl;
    
    shared_ptr<FrameFetcherImpl> self = shared_from_this();
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
FrameFetcherImpl::fetchGopDelta(const boost::shared_ptr<const SlotSegment>& segment)
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

            shared_ptr<FrameFetcherImpl> self = shared_from_this();
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
FrameFetcherImpl::checkReadyDecode()
{
    if (state_ == FrameFetcher::Fetching)
    {
        // LogInfoC << "checking " << fetchingTasks_.size() << " tasks..." << std::endl;

        // check if we have all that's needed to decoding...
        bool allFetched = true;
        for (auto t:fetchingTasks_)
            allFetched &= (t.second->getState() == FrameFetchingTask::Completed);

        if (allFetched)
        {
            state_ = FrameFetcher::Decoding;
            decode();
        }
    }
}

void
FrameFetcherImpl::decode()
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
        shared_ptr<FrameFetcherImpl> self = shared_from_this();
        int needToDecode = fetchingTasks_.size();
        OnDecodedImage onDecoded = 
            [&needToDecode, self, this](const FrameInfo& fi, const WebRtcVideoFrame& f){
                --needToDecode;

                if (needToDecode == 0)
                {
                    uint8_t* buffer = onBufferAllocate_(self, f.width(), f.height());
                    if (buffer)
                    {
                        state_ = FrameFetcher::Completed;

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
FrameFetcherImpl::reset()
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
FrameFetcherImpl::halt(std::string reason)
{
    if (state_ != FrameFetcher::Failed)
    {
        state_ = FrameFetcher::Failed;
        onFetchFailure_(shared_from_this(), reason);
        reset();
    }
}

VideoCoderParams
FrameFetcherImpl::setupDecoderParams(const boost::shared_ptr<ImmutableVideoFramePacket>& fp) const
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