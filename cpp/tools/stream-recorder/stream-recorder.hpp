//
// stream-recorder.hpp
//
//  Created by Peter Gusev on 9 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __stream_recorder_hpp__
#define __stream_recorder_hpp__

#include <boost/shared_ptr.hpp>

namespace ndn {
    class Face;
    class KeyChain;
}

namespace ndnlog {
    namespace new_api {
        class Logger;
    }
}

namespace ndnrtc {
    class MetaFetcher;
    class NamespaceInfo;
    class StorageEngine;
    class StreamRecorderImpl;

    /**
     * This class fetches (live) ndnrtc stream and stores all associated data packets 
     * into persistent storage for later retrieval.
     * StreamRecorder may operate in several modes:
     *  -- ffwd: fetch stream starting from the most recent data, until stopped
     *  -- bckwd: fetch stream startging from the most recent data moving backwards
     *  -- ffwd && bckwd: fetch stream strarting from the most recent data, in 
     *      both directions (forward and backwards)
     * StreamRecorder can be initialized with a seed frame, which will be 
     * used to as a starting frame for fetching (instead of using most recent 
     * frame).
     * StreamRecroder can be intialized for fetching N frames. In this case, only
     * data packets associated with N frames (Key and Delta) will be fetched.
     */
    class StreamRecorder {
        public: 

        typedef enum _FetchDirection {
            Forward = 1 << 0,
            Backward = 1 << 1
        } FetchDirection;

        typedef struct _FetchSettings {
            uint16_t lifetime_;
            uint8_t direction_;
            uint32_t seedFrame_;
            uint32_t recordLength_;
            uint8_t pipelineSize_;
            uint32_t streamMetaFetchInterval_, threadMetaFetchInterval_;
        } FetchSettings;

        typedef struct _Stats {
            size_t manifestsStored_;
            size_t streamMetaStored_, threadMetaStored_;
            size_t keyStored_, deltaStored_;
            uint32_t latestKeyFetched_, latestDeltaFetched_;
            uint32_t latestKeyRequested_, latestDeltaRequested_;
            uint64_t totalSegmentsStored_;
            size_t deltaFailed_, keyFailed_;
            size_t pendingFrames_;
        } Stats;

        static const FetchSettings Default;

        /**
         * Construcor:
         * @param io io_service which will be used for asynchronous Interest/Data processing.
         * @param storageEngine Persisten storage engine to store data in.
         * @param streamPrefix ndnrtc stream prefix.
         * @param face Face object used for expressing interests.
         * @param keyChain KeyChain object used to verify incoming data. If ommitted, 
         *                  a default keychain with NoVerify policy will be used.
         */
        StreamRecorder(const boost::shared_ptr<StorageEngine>& storageEngine, 
                        const NamespaceInfo& ninfo,
                        const boost::shared_ptr<ndn::Face>& face, 
                        const boost::shared_ptr<ndn::KeyChain> keyChain);
        ~StreamRecorder(){ pimpl_.reset(); }

        /**
         * Start stream fetching.
         * @param directionMask Defines direction for retrieving data. If ommitted, will fetch newly generated frames.
         * @param seedFrame Initial frame to start fetching from. If ommitted, start from the most recent frame.
         * @param nFrames Number of frames to fetch. If ommitted, will keep fetching until stopped.
         */
        void start(const FetchSettings& settings = Default);
        void stop();

        bool isFetching();
        const std::string getStreamPrefix() const;
        const std::string getThreadName() const;
        void setLogger(const boost::shared_ptr<ndnlog::new_api::Logger>& logger);
        const Stats& getCurrentStats() const;

        private:
        boost::shared_ptr<StreamRecorderImpl> pimpl_;
    };
}

#endif