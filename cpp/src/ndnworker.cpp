//
//  ndnworker.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/6/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "ndnworker.h"

#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsthread-tasks.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - creation
NdnWorker::NdnWorker(const char *hub, const int port, INdnWorkerDelegate *delegate):DataClosure()
{
    delegate_ = delegate;
    face_ = new Face(hub, port, shared_ptr<UdpTransport>(new UdpTransport()));
}
NdnWorker::~NdnWorker()
{
    spinThread_->Shutdown();
}
//********************************************************************************
void NdnWorker::expressInterestAsync(const char *interest)
{
    nsresult rv;
    
    gotContent_ = false;
    // provide shared_ptr self as a closure
    TRACE("expressing interest");
    face_->expressInterest(Name(interest), this);
    
    nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
    
    if (!spinThread_)
        rv = tm->NewThread(0, 0, getter_AddRefs(spinThread_));
    
    
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIRunnable> spinTask = new NdnWorkerSpinTask(*this);
        spinThread_->Dispatch(spinTask, nsIThread::DISPATCH_NORMAL);
    }
    else
        ERROR("spin thread creation failed");
}

void NdnWorker::dispatchData(shared_ptr<Data> &data)
{
    TRACE("should dispatch callback on main thread here");
    
    NS_DispatchToMainThread(new NdnWorkerDispatchDataTask(delegate_,data));
    
}

