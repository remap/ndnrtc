//
//  nsspin-task.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/7/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "nsthread-tasks.h"
#include "ndnworker.h"

using namespace ndnrtc;

NS_IMETHODIMP NdnWorkerSpinTask::Run()
{
    while (!currentWorker_->gotContent_)
    {
        cout << "fetching" << endl;
        currentWorker_->getTransport()->tempReceive();
    }
    cout << "got something" << endl;
    
    return NS_OK;
}

NS_IMETHODIMP NdnWorkerDispatchDataTask::Run()
{
    cout << "calling delegate" << endl;
    delegate_->onDataReceived(data_);
    
    return NS_OK;
}