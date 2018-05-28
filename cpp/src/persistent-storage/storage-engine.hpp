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

    class StorageEngine {
    public:
        StorageEngine(std::string dpPath);
        ~StorageEngine();

        void put(const boost::shared_ptr<const ndn::Data>& data);
        boost::shared_ptr<ndn::Data> get(const ndn::Name& dataName);

    private:
        boost::shared_ptr<StorageEngineImpl> pimpl_;
    };
}

#endif