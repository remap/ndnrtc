//
// stream-recorder.cpp
//
//  Created by Peter Gusev on 9 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "stream-recorder.hpp"

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/face.hpp>

#include "../../include/name-components.hpp"
#include "../../include/storage-engine.hpp"

#include "../../src/frame-buffer.hpp"
#include "../../src/network-data.hpp"
#include "../../src/ndnrtc-object.hpp"
#include "../../src/segment-fetcher.hpp"
#include "../../src/persistent-storage/fetching-task.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndn;

const StreamRecorder::FetchSettings 
StreamRecorder::Default = {3000, StreamRecorder::FetchDirection::Forward, 0, 0, 5, 100, 30};

namespace ndnrtc {
    class StreamRecorderImpl : public NdnRtcComponent
    {
        friend class StreamRecorder;
        public:
            StreamRecorderImpl(const boost::shared_ptr<StorageEngine>& storageEngine, 
                            const NamespaceInfo& ninfo,
                            const boost::shared_ptr<Face>& face, 
                            const boost::shared_ptr<KeyChain> keyChain);
            ~StreamRecorderImpl(){}

            /**
             * Start stream fetching.
             */
            void start(const StreamRecorder::FetchSettings& setting);
            void stop();

            bool isFetching() { return isFetching_; }
            const string getStreamPrefix() const { return ninfo_.getPrefix(prefix_filter::Stream).toUri(); }
            const string getThreadName() const { return ninfo_.threadName_; }

        private:
            const NamespaceInfo ninfo_;
            boost::shared_ptr<StorageEngine> storage_;
            boost::shared_ptr<Face> face_;
            boost::shared_ptr<KeyChain> keyChain_;
            bool isFetching_, isFetchingStream_;
            StreamRecorder::FetchSettings settings_;
            int32_t pipelineReserve_;
            pair<uint32_t, uint32_t> fetchIndex_;

            FetchingTask::Settings fetchTaskSettings_;
            boost::shared_ptr<IFetchMethod> frameFetchMethod_;
            map<Name, boost::shared_ptr<FrameFetchingTask>> fetchingTasks_;
            StreamRecorder::Stats stats_;

            void fetchStreamMeta();
            void fetchThreadMeta();
            void initiateStreamFetching(const Blob& meta);
            void requestFrame(const NamespaceInfo& frameInfo);
            void requestNextFrame(const NamespaceInfo& fetchedFrame);

            void store(const boost::shared_ptr<const Data>&d);
    };
}

// ***
StreamRecorder::StreamRecorder(const boost::shared_ptr<StorageEngine>& storageEngine, 
                        const NamespaceInfo& ninfo,
                        const boost::shared_ptr<Face>& face, 
                        const boost::shared_ptr<KeyChain> keyChain):
pimpl_(boost::make_shared<StreamRecorderImpl>(storageEngine, ninfo, face, keyChain)){}
void StreamRecorder::start(const StreamRecorder::FetchSettings& settings) { pimpl_->start(settings); }
void StreamRecorder::stop() { pimpl_->stop(); }
bool StreamRecorder::isFetching() { return pimpl_->isFetching_; }
const string StreamRecorder::getStreamPrefix() const { return pimpl_->ninfo_.getPrefix(prefix_filter::Stream).toUri(); }
const string StreamRecorder::getThreadName() const { return pimpl_->ninfo_.threadName_; }
void StreamRecorder::setLogger(const boost::shared_ptr<ndnlog::new_api::Logger>& logger) { pimpl_->setLogger(logger); }
const StreamRecorder::Stats& StreamRecorder::getCurrentStats() const { return pimpl_->stats_; }
// ***

StreamRecorderImpl::StreamRecorderImpl(const boost::shared_ptr<StorageEngine>& storageEngine, 
                        const NamespaceInfo& ninfo,
                        const boost::shared_ptr<Face>& face, 
                        const boost::shared_ptr<KeyChain> keyChain):
    storage_(storageEngine), face_(face), keyChain_(keyChain),
    ninfo_(ninfo), isFetching_(false), isFetchingStream_(false)
{
    if (ninfo_.streamType_ == MediaStreamParams::MediaStreamType::MediaStreamTypeAudio)
        throw runtime_error("audio streams are not supported yet");

    description_ = "recorder-"+ninfo.streamName_+":"+ninfo.threadName_;
    frameFetchMethod_ = boost::make_shared<FetchMethodRemote>(face_);
}

void 
StreamRecorderImpl::start(const StreamRecorder::FetchSettings& settings)
{
    if (isFetching_ || isFetchingStream_)
        throw runtime_error("Stream recorder is already fetching");

    settings_ = settings;
    pipelineReserve_ = settings_.pipelineSize_;
    fetchTaskSettings_ = {3, settings_.lifetime_};
    memset((void*)&stats_, 0, sizeof(StreamRecorder::Stats));

    LogInfoC << "recording direction: " 
        << ((settings_.direction_ & StreamRecorder::FetchDirection::Forward) && (settings_.direction_ & StreamRecorder::FetchDirection::Backward) ? "both" : 
            (settings_.direction_ & StreamRecorder::FetchDirection::Forward ? "forward" : "backward"))
        << ", seed frame: " << settings_.seedFrame_
        << ", record length: " << settings_.recordLength_
        << ", interest lifetime: " << settings_.lifetime_
        << ", pipeline: " << settings_.pipelineSize_ << endl;

    isFetching_ = true;
    fetchStreamMeta();
    fetchThreadMeta();
}

void 
StreamRecorderImpl::stop()
{
    isFetching_ = false;
    isFetchingStream_ = false;
    for (auto t:fetchingTasks_)
        t.second->cancel();
}

void 
StreamRecorderImpl::fetchStreamMeta(){
    Interest i(ninfo_.getPrefix(prefix_filter::Stream).append(NameComponents::NameComponentMeta), settings_.lifetime_);
    boost::shared_ptr<StreamRecorderImpl> me = dynamic_pointer_cast<StreamRecorderImpl>(shared_from_this());

    // LogDebugC << "fetching " << i.getName() << endl;

    SegmentFetcher::fetch(*face_, i, keyChain_.get(), 
                        [me,this](const Blob &content,
                                  const vector<ValidationErrorInfo>&,
                                  const vector<boost::shared_ptr<Data>>& contentData){
                                    for (auto d:contentData)
                                    {
                                        // LogTraceC << "store " << d->getName() << endl; 
                                        store(d);
                                    }
                                    stats_.streamMetaStored_++;

                                    if (isFetching_)
                                        face_->callLater(settings_.streamMetaFetchInterval_, 
                                                         boost::bind(&StreamRecorderImpl::fetchStreamMeta, me));
                                  },
                        [me, this](SegmentFetcher::ErrorCode code, 
                                   const string& msg){
                                       LogErrorC << "failed to fetch stream meta: " << msg << endl;
                                       if (isFetching_) 
                                            me->fetchStreamMeta();
                                   });
}

void 
StreamRecorderImpl::fetchThreadMeta(){
    Interest i(ninfo_.getPrefix(prefix_filter::ThreadNT).append(NameComponents::NameComponentMeta), settings_.lifetime_);
    boost::shared_ptr<StreamRecorderImpl> me = dynamic_pointer_cast<StreamRecorderImpl>(shared_from_this());

    // LogDebugC << "fetching " << i.getName() << endl;

    SegmentFetcher::fetch(*face_, i, keyChain_.get(), 
                        [me,this](const Blob &content,
                                  const vector<ValidationErrorInfo>&,
                                  const vector<boost::shared_ptr<Data>>& contentData){
                                    for (auto d:contentData)
                                    {
                                        // LogTraceC << "store " << d->getName() << endl; 
                                        store(d);
                                    }
                                    stats_.threadMetaStored_++;

                                    if (isFetching_)
                                    {
                                        if (!isFetchingStream_)
                                            initiateStreamFetching(content);
                                        face_->callLater(settings_.threadMetaFetchInterval_, 
                                                         boost::bind(&StreamRecorderImpl::fetchThreadMeta, me));
                                    }
                                  },
                        [me, this](SegmentFetcher::ErrorCode code, 
                                   const string& msg){
                                       LogErrorC << "failed to fetch stream meta: " << msg << endl;
                                       if (isFetching_) 
                                            me->fetchStreamMeta();
                                   });
}

void
StreamRecorderImpl::initiateStreamFetching(const Blob &metaBlob)
{
    isFetchingStream_ = true;
    fetchIndex_.first = settings_.seedFrame_;
    fetchIndex_.second = 0;

    if (fetchIndex_.first == 0)
    {
        // extract latest key and delta numbers from metadata
        if (ninfo_.streamType_ == MediaStreamParams::MediaStreamTypeVideo)
        {
            ImmutableHeaderPacket<DataSegmentHeader> packet(metaBlob);
            NetworkData nd(packet.getPayload().size(), packet.getPayload().data());
            VideoThreadMeta threadMeta(boost::move(nd));

            fetchIndex_.first = threadMeta.getSeqNo().second;
            fetchIndex_.second = threadMeta.getSeqNo().first;

            NamespaceInfo frameInfo = ninfo_;
            frameInfo.class_ = SampleClass::Key;
            frameInfo.sampleNo_ = fetchIndex_.first;
            fetchIndex_.first++;
            requestFrame(frameInfo);

            while (pipelineReserve_)
            {
                frameInfo.class_ = SampleClass::Delta;
                frameInfo.sampleNo_ = fetchIndex_.second;
                fetchIndex_.second++;
                requestFrame(frameInfo);
            }
        }
        else
            throw runtime_error("audio streams are not supported yet!");
    }
    else
        throw runtime_error("Seed frame is not supported yet!");
}

void StreamRecorderImpl::requestFrame(const NamespaceInfo& frameInfo)
{
    LogDebugC << "request " << frameInfo.getSuffix(suffix_filter::Thread) 
             << "(" << frameInfo.sampleNo_ << ")" << endl;

    if (frameInfo.class_ == SampleClass::Key)
        stats_.latestKeyRequested_ = frameInfo.sampleNo_;
    else
        stats_.latestDeltaRequested_ = frameInfo.sampleNo_;

    boost::shared_ptr<StreamRecorderImpl> me = dynamic_pointer_cast<StreamRecorderImpl>(shared_from_this());
    Name fName = frameInfo.getPrefix(prefix_filter::Sample);
    boost::shared_ptr<FrameFetchingTask> task = 
        boost::make_shared<FrameFetchingTask>(
            fName,
            frameFetchMethod_, 
            [this, me, frameInfo](const boost::shared_ptr<const FetchingTask>& t, 
             const boost::shared_ptr<const BufferSlot>& slot)
            { // onComplete
                fetchingTasks_.erase(frameInfo.getSuffix(suffix_filter::Thread));
                pipelineReserve_++;

                for (auto s:slot->getFetchedSegments())
                    store(s->getData()->getData());

                if (frameInfo.class_ == SampleClass::Delta)
                {
                    stats_.latestDeltaFetched_ = frameInfo.sampleNo_;
                    stats_.deltaStored_++;
                }
                else
                {
                    stats_.latestKeyFetched_ = frameInfo.sampleNo_;
                    stats_.keyStored_++;
                }

                LogDebugC << "stored frame " << frameInfo.getSuffix(suffix_filter::Thread) 
                         << "(" << frameInfo.sampleNo_ << ")" << endl;

                if (isFetching_)
                    requestNextFrame(frameInfo);
            },
            [this, me, frameInfo](const boost::shared_ptr<const FetchingTask>& t, 
             std::string msg)
            { // onFailed
                LogErrorC << "couldn't fetch frame " << t->getFrameName() 
                            << ": " << msg << endl;

                fetchingTasks_.erase(frameInfo.getSuffix(suffix_filter::Thread));
                pipelineReserve_++;

                if (frameInfo.class_ == SampleClass::Delta)
                    stats_.deltaFailed_++;
                else
                    stats_.keyFailed_++;

                if (isFetching_)
                    requestNextFrame(frameInfo);
            },
            fetchTaskSettings_,
            [this, me]
            (const boost::shared_ptr<const FetchingTask>&, 
             const boost::shared_ptr<const SlotSegment>&)
            { // onFirstSegment
                
            });
    fetchingTasks_[frameInfo.getSuffix(suffix_filter::Thread)] = task;
    task->setLogger(logger_);
    task->start();
    pipelineReserve_--;
    stats_.pendingFrames_ = fetchingTasks_.size();

    // fetch manifest, too
    Interest i(fName.append(NameComponents::NameComponentManifest), settings_.lifetime_);
    SegmentFetcher::fetch(*face_, i, keyChain_.get(), 
                        [me, this, frameInfo](const Blob &content,
                                  const vector<ValidationErrorInfo>&,
                                  const vector<boost::shared_ptr<Data>>& contentData){
                                    for (auto d:contentData)
                                        store(d);
                                    stats_.manifestsStored_++;

                                    LogDebugC << "stored manifest for " << frameInfo.getSuffix(suffix_filter::Sample) << endl;
                                  },
                        [me, this, frameInfo](SegmentFetcher::ErrorCode code, 
                                   const string& msg){
                                       LogWarnC << "failed to fetch manifest for " << frameInfo.getSuffix(suffix_filter::Sample) 
                                                << " :" << msg << endl;
                                   });
}

void
StreamRecorderImpl::requestNextFrame(const NamespaceInfo& fetchedFrame)
{
    NamespaceInfo nextFrame(fetchedFrame);
    if (nextFrame.class_ == SampleClass::Key)
        nextFrame.sampleNo_ = fetchIndex_.first++;
    else
        nextFrame.sampleNo_ = fetchIndex_.second++;

    requestFrame(nextFrame);
}

void
StreamRecorderImpl::store(const boost::shared_ptr<const Data>&d)
{
    storage_->put(d);
    stats_.totalSegmentsStored_++;
}