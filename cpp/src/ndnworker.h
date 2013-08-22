//
//  ndnworker.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/6/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__ndnworker__
#define __ndnrtc__ndnworker__

#include <boost/enable_shared_from_this.hpp>
#include "ndnrtc-common.h"
#include "data-closure.h"
#include "nsThreadUtils.h"

using namespace ndn;
using namespace ptr_lib;

namespace ndnrtc
{
    class INdnWorkerDelegate;
    /**
     * NDN worker class operates on a separate thread and dispatches callbacks on a main thread
     */
    class NdnWorker : public DataClosure, public ptr_lib::enable_shared_from_this<NdnWorker>
    {
    public:
        // construction/desctruction
        NdnWorker(const char *hub, const int port, INdnWorkerDelegate *delegate);
        ~NdnWorker();
        
        // public methods go here
        void expressInterestAsync(const char *interest);
        void publishData(const char *prefix, const unsigned char *data, unsigned int datalen);
        void connect();
        bool isConnected(){ return isConnected_; }
        Transport*  getTransport() const{ return face_->getTransport().get(); }
        Face* getFace() const {return face_; }
        void tempReceive();

    private:
        // private attributes go here
        Face *face_;
        INdnWorkerDelegate *delegate_;
        nsCOMPtr<nsIThread> spinThread_;
        bool isConnected_;
        
        // private methods go here
        void dispatchData(shared_ptr<Data> &data);
        shared_ptr<NdnWorker> sharedThis(){ return shared_from_this(); }
    };
    
    class INdnWorkerDelegate {
    public:
        virtual void onDataReceived(shared_ptr<Data> &data) = 0;
    };
}

#endif /* defined(__ndnrtc__ndnworker__) */
