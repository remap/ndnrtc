//
// consumer-storage.hpp
//
//  Created by Peter Gusev on 8 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __consumer_storage_hpp__
#define __consumer_storage_hpp__

#include <frame-buffer.hpp>

namespace ndn {
    class Face;
    class KeyChain;
}

namespace ndnrtc {

class StorageEngine;
class RemoteStream;
class 

/** 
 * This class provides storage support for consumers.
 * It stores every received packet into a persistent storage.
 */
class ConsumerStorage : public NdnRtcComponent, public IBufferObserver {
    public: 
    ConsumerStorage(const boost::shared_ptr<StorageEngine>&, const std::string& streamPrefix,
        const boost::shared_ptr<ndn::Face>& face, 
        const boost::shared_ptr<ndn::KeyChain> keyChain = boost::make_shared<ndn::KeyChain>(nullptr)){}
    
    private:
    // IBufferObserver interface
    virtual void onNewRequest(const boost::shared_ptr<BufferSlot>&) {}
    virtual void onNewData(const BufferReceipt& receipt) {}
    virtual void onReset() {}
};

}

#endif
