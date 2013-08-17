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
        try {
            TRACE("fetching");
            currentWorker_->getFace()->processEvents();
        }
        catch (std::exception &e)
        {
            ERROR("exception: %s", e.what());
        }
    }
    TRACE("got something");
    
    return NS_OK;
}

NS_IMETHODIMP NdnWorkerDispatchDataTask::Run()
{
    TRACE("calling delegate");
    delegate_->onDataReceived(data_);
    
    return NS_OK;
}