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
#include "frame-data.hpp"

namespace ndnrtc {
    class StorageEngine;
    class FrameFetcher;
    class BufferSlot;
    class SlotSegment;
    class FrameFetchingTask;
    class IFetchMethod;

    class FrameFetcherImpl;
    class IFrameFetcher;

    typedef boost::function<uint8_t*(const boost::shared_ptr<IFrameFetcher>&, 
                                     int width, int height)> OnBufferAllocate;
    typedef boost::function<void(const boost::shared_ptr<IFrameFetcher>&,
                                 const FrameInfo&, int nFramesFetched,
                                 int width, int height, const uint8_t* buffer)> OnFrameFetched;
    typedef boost::function<void(const boost::shared_ptr<IFrameFetcher>&,
                                 std::string reason)> OnFetchFailure;

    class IFrameFetcher {
    public:
        virtual void fetch(const ndn::Name& frameName, 
                           OnBufferAllocate onBufferAllocate,
                           OnFrameFetched onFrameFetched,
                           OnFetchFailure onFetchFailure) = 0;
        virtual const ndn::Name& getName() const = 0;
        virtual ~IFrameFetcher(){}
    };

    class FrameFetcher : public IFrameFetcher,
                         public ndnlog::new_api::ILoggingObject {
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
        ~FrameFetcher(){}

        void fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure);
        const ndn::Name& getName() const;
        State getState() const;
        void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

    private:
       boost::shared_ptr<FrameFetcherImpl> pimpl_;
    };
}

#endif