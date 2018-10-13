//
// storage-engine.cpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "storage-engine.hpp"

#include <ndn-cpp/name.hpp>
#include <ndn-cpp/data.hpp>
#include <ndn-cpp/util/blob.hpp>

#if HAVE_PERSISTENT_STORAGE

#ifndef __ANDROID__ // use RocksDB on linux and macOS
    
    #include <rocksdb/db.h>
    namespace db_namespace = rocksdb;

#else // for Android - use LevelDB

    #include <leveldb/db.h>
    namespace db_namespace = leveldb;

#endif

#endif

using namespace ndnrtc;
using namespace ndn;
using namespace boost;

//******************************************************************************
namespace ndnrtc {

class StorageEngineImpl {
    public:
    #if HAVE_PERSISTENT_STORAGE
        StorageEngineImpl(std::string dbPath): dbPath_(dbPath), db_(nullptr)
        {
        }
    #else
        StorageEngineImpl(std::string dbPath) {
            throw std::runtime_error("The library is not copmiled with persistent storage support.");
        }
    #endif

        ~StorageEngineImpl()
        {
            close();
        }

        bool open()
        {
#if HAVE_PERSISTENT_STORAGE
            db_namespace::Options options;
            options.create_if_missing = true;

            db_namespace::Status status = db_namespace::DB::Open(options, dbPath_, &db_);
            return status.ok();
#else
            return false;
#endif
        }

        void close()
        {
#if HAVE_PERSISTENT_STORAGE
            if (db_)
                delete db_;
#endif
        }

        bool put(const shared_ptr<const Data>& data)
        {
#if HAVE_PERSISTENT_STORAGE
            if (!db_)
                throw std::runtime_error("DB is not open");

            db_namespace::Status s = 
                db_->Put(db_namespace::WriteOptions(),
                         data->getName().toUri(),
                         db_namespace::Slice((const char*)data->wireEncode().buf(),
                                             data->wireEncode().size()));
            return s.ok();
#else
            return false;
#endif
        }

        shared_ptr<Data> get(const Name& dataName)
        {
#if HAVE_PERSISTENT_STORAGE
            if (!db_)
                throw std::runtime_error("DB is not open");

            static std::string dataString;
            db_namespace::Status s = db_->Get(db_namespace::ReadOptions(),
                                              dataName.toUri(),
                                              &dataString);
            if (s.ok())
            {
                shared_ptr<Data> data = make_shared<Data>();
                data->wireDecode((const uint8_t*)dataString.data(), dataString.size());
                
                return data;
            }
#endif
            return shared_ptr<Data>(nullptr);
        }

    private:
        std::string dbPath_;
#if HAVE_PERSISTENT_STORAGE
        db_namespace::DB* db_;
#endif
};

}

//******************************************************************************
StorageEngine::StorageEngine(std::string dbPath):
    pimpl_(boost::make_shared<StorageEngineImpl>(dbPath))
{
    pimpl_->open();
}

StorageEngine::~StorageEngine()
{
    pimpl_->close();
}

void StorageEngine::put(const shared_ptr<const Data>& data)
{
    pimpl_->put(data);
}

boost::shared_ptr<Data> 
StorageEngine::get(const Name& dataName)
{
    return pimpl_->get(dataName);
}

