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
        NdnWorker(const char *hub, const int port, INdnWorkerDelegate *delegate);
        ~NdnWorker();
        
        void expressInterestAsync(const char *interest);
        
        Transport*  getTransport() const{
            return face_->getTransport().get();
        }

    private:
        void dispatchData(shared_ptr<Data> &data);
        
        shared_ptr<NdnWorker> sharedThis()
        {
            return shared_from_this();
        }
        
        Face *face_;
        INdnWorkerDelegate *delegate_;
        nsCOMPtr<nsIThread> spinThread_;
    };
    
    class INdnWorkerDelegate {
    public:
        virtual void onDataReceived(shared_ptr<Data> &data) = 0;
    };
}

#endif /* defined(__ndnrtc__ndnworker__) */
