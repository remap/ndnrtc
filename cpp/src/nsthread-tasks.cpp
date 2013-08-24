//
//  nsspin-task.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/7/13
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