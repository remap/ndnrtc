//
//  nsspin-task.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/7/13
//

#ifndef __ndnrtc__nsspin_task__
#define __ndnrtc__nsspin_task__

#include "ndnrtc-common.h"
#include "nsThreadUtils.h"
#include "ndnworker.h"
#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"

namespace ndnrtc
{
    class NdnWorkerSpinTask : public nsRunnable
    {
    public:
        NdnWorkerSpinTask(NdnWorker &ndnWorker)
        {
            currentWorker_ = &ndnWorker;
        }
        
        NS_METHOD Run();
        
    private:
        NdnWorker *currentWorker_;
    };
    
    class NdnWorkerDispatchDataTask : public nsRunnable
    {
    public:
        NdnWorkerDispatchDataTask(INdnWorkerDelegate *delegate, shared_ptr<Data> &data)
        {
            delegate_ = delegate;
            data_ = data;
        }

        NS_METHOD Run();
    private:
        shared_ptr<Data> data_;
        INdnWorkerDelegate *delegate_;
    };
}

#endif /* defined(__ndnrtc__nsspin_task__) */
