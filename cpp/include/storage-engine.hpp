//
// storage-engine.hpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __storage_engine_hpp__
#define __storage_engine_hpp__

#include <boost/shared_ptr.hpp>

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
        StorageEngine(std::string dpPath);
        ~StorageEngine();

        /**
         * Puts new data packet into the storage.
         * Data is saved asynchronously, so the call returns immediately.
         * The call is thread-safe.
         */
        void put(const std::shared_ptr<const ndn::Data>& data);

        /**
         * Tries to retrieve data from persistent storage. 
         * The call is synchronous. 
         * If data is not present in the persistent storage, returned pointer
         * is invalid.
         */
        std::shared_ptr<ndn::Data> get(const ndn::Name& dataName);

    private:
        std::shared_ptr<StorageEngineImpl> pimpl_;
    };
}

#endif
