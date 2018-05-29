//
// frame-fetcher.hpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __frame_fetcher_hpp__

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <ndn-cpp/name.hpp>

#include "interfaces.hpp"
#include "params.hpp"
#include "simple-log.hpp"
#include "name-components.hpp"
#include "persistent-storage/fetching-task.hpp"
#include "frame-data.hpp"

namespace ndnrtc {
    class StorageEngine;
    class FrameFetcher;
    class BufferSlot;
    class SlotSegment;
    class FrameFetchingTask;
    class IFetchMethod;

    typedef boost::function<uint8_t*(const boost::shared_ptr<FrameFetcher>&, 
                                     int width, int height)> OnBufferAllocate;
    typedef boost::function<void(const boost::shared_ptr<FrameFetcher>&,
                                 const FrameInfo&, int nFramesFetched,
                                 int width, int height, const uint8_t* buffer)> OnFrameFetched;
    typedef boost::function<void(const boost::shared_ptr<FrameFetcher>&,
                                 std::string reason)> OnFetchFailure;

    class FrameFetcher : public  ndnlog::new_api::ILoggingObject,
                         public boost::enable_shared_from_this<FrameFetcher> {
    public:
        enum State {
            Idle,
            Fetching,
            Decoding,
            Failed,
            Completed
        };

        FrameFetcher(const boost::shared_ptr<StorageEngine>& storage);
        // FrameFetcher(const boost::shared_ptr<LocalVideoStream>& localStream);
        // FrameFetcher(std::string streamPrefix);
        ~FrameFetcher();

        void fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure);

        State getState() const { return state_; }

    private:
        State state_;
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

#endif