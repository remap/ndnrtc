//
// storage-engine.hpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __storage_engine_hpp__
#define __storage_engine_hpp__

#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <ndn-cpp/name.hpp>

namespace ndn {
    class Data;
    class Name;
}

namespace ndnrtc {
    class StorageEngineImpl;

    /**
     * This is a wrapper for the persistent key-value storage of data packets.  
     */
    class StorageEngine {
    public:
        StorageEngine(std::string dpPath, bool readOnly = false);
        ~StorageEngine();

        /**
         * Puts new data packet into the storage.
         * Data is saved asynchronously, so the call returns immediately.
         * The call is thread-safe.
         */
        void put(const boost::shared_ptr<const ndn::Data>& data);
        void put(const ndn::Data& data);

        /**
         * Tries to retrieve data from persistent storage. 
         * The call is synchronous. 
         * If data is not present in the persistent storage, returned pointer
         * is invalid.
         */
        boost::shared_ptr<ndn::Data> get(const ndn::Name& dataName);

        /**
         * Tries to retrieve data from persistent storage according to the 
         * interest received. 
         * The call is synchronous. 
         * If data is not present in the persistent storage, returned pointer
         * is invalid.
         */
        boost::shared_ptr<ndn::Data> read(const ndn::Interest& interest);

        /**
         * Scans DB for longest common prefixes. May take a while, depending on 
         * DB size.
         * @param io io_service to use for asynchronous scanning
         * @param onCompleted Callback called upon completion. Passes list of 
         *                    longest common prefixes discovered in the database.
         */ 
        void scanForLongestPrefixes(boost::asio::io_service& io, 
            boost::function<void(const std::vector<ndn::Name>&)> onCompleted);

        /**
         * Returns approximate storage payload (all the values) size in bytes.
         */
        const size_t getPayloadSize() const;
        /**
         * Returns total number of keys in this KV-storage.
         */
        const size_t getKeysNum() const;

    private:
        boost::shared_ptr<StorageEngineImpl> pimpl_;
    };
}

#endif