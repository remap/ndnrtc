//
//  nsspin-task.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/7/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__nsspin_task__
#define __ndnrtc__nsspin_task__

#include "ndnrtc-common.h"
#include "nsThreadUtils.h"
#include "ndnworker.h"


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
