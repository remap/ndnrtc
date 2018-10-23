//
// frame-fetcher.hpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __frame_fetcher_hpp__

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "interfaces.hpp"
#include "params.hpp"
#include "simple-log.hpp"
#include "name-components.hpp"
#include "frame-data.hpp"

namespace ndn {
    class Name;
    class Face;
    class KeyChain;
}

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

    /**
     * Fetches frames from provided persistent storage by names. Once frame is 
     * fetched, returns ARGB buffer for this frame through provided callbacks.
     * Frame fetching involves expressing interests for all segments of a frame 
     * and segments of all preceding frames in the frame's GOP.
     * Frame fetcher will figure out number of segments for requested frame once 
     * it has fetched first segment.
     * If requested frame is a Delta frame (type of the frame is derived from 
     * its' name), frame fetcher will automatically fetch all necessary preceding
     * delta frames and its' corresponding GOP key frame in order to decode frame
     * correctly. For example, if delta frame #20 is requested, 19 preceding delta 
     * frames and 1 key frame must be fetched before decoding of #20 can be 
     * started.
     * If requested frame is a Key frame, no additional frames will be fetched.
     */
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

        /**
         * Fetches frames from local persistent storage.
         */
        FrameFetcher(const boost::shared_ptr<StorageEngine>& storage);

        /**
         * Fetches frames by expressing interests on the provided face object.
         */ 
        FrameFetcher(const boost::shared_ptr<ndn::Face>&, 
                     const boost::shared_ptr<ndn::KeyChain>&);
        ~FrameFetcher(){}

        /**
         * Fetches frame by its' name. Returns contents of the frame in ARGB 
         * buffer through the given callback or reports a failure.
         * @param frameName The name of a frame to fetch
         * @param onBufferAllocate Once frame has been fetched and ready to be 
         *  decoded, client code needs to allocate a buffer for the deocded data
         *  by returning a byte pointer when this callback is called.
         * @param onFrameFetched Once frame has been decoded, this callback will 
         *  be called to notify client code of successful fetching.
         * @param onFetchFailure If frame fetching fails, client code will be 
         *  notified using this callback.
         */
        void fetch(const ndn::Name& frameName, 
                   OnBufferAllocate onBufferAllocate,
                   OnFrameFetched onFrameFetched,
                   OnFetchFailure onFetchFailure);

        /**
         * Returns name of a frame that is currently being fetched.
         */
        const ndn::Name& getName() const;

        /**
         * Returns current state of the frame fetching process. 
         */
        State getState() const;

        void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

    private:
       boost::shared_ptr<FrameFetcherImpl> pimpl_;
    };
}

#endif