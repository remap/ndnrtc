//  Copyright (C) 2013 Regents of the University of California
//  Author(s): Peter Gusev <gpeetonn@gmail.com>
//  See the LICENSE file for licensing information.
//

// Created: 8/6/2013
// Last updated: 7/29/2013

#include "ndnworker.h"

#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsthread-tasks.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - construction/destruction
NdnWorker::NdnWorker(const char *hub, const int port, INdnWorkerDelegate *delegate):DataClosure()
{
    delegate_ = delegate;
    face_ = new Face(hub, port, shared_ptr<UdpTransport>(new UdpTransport()));
}
NdnWorker::~NdnWorker()
{
    TRACE("shutting down spin thread");
    spinThread_->Shutdown();
    spinThread_ = NULL;
    delete face_;
}
//********************************************************************************
#pragma mark - public
void NdnWorker::expressInterestAsync(const char *interest)
{
    nsresult rv;
    
    gotContent_ = false;
    // provide shared_ptr self as a closure
    TRACE("expressing interest");
    face_->expressInterest(Name(interest), this);
    isConnected_ = true;
    
    nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
    
    if (!spinThread_)
    {
        INFO("creating new spin thread");
        rv = tm->NewThread(0, 0, getter_AddRefs(spinThread_));
    }
    
    
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIRunnable> spinTask = new NdnWorkerSpinTask(*this);
        spinThread_->Dispatch(spinTask, nsIThread::DISPATCH_NORMAL);
    }
    else
        ERROR("spin thread creation failed");
}
void NdnWorker::publishData(const char *prefix, const unsigned char *data, unsigned int datalen)
{
    if (!isConnected())
        connect();
    
    Name name(prefix);
    Data ndnData(name);
    
    ndnData.setContent(data, datalen);
    ndnData.getSignedInfo().setTimestampMilliseconds(time(NULL) * 1000.0);
    
    KeyChain::defaultSign(ndnData);
    shared_ptr<vector<unsigned char>> encodedData = ndnData.wireEncode();
    
    getTransport()->send(&encodedData->front(), encodedData->size());
}
void NdnWorker::connect()
{
    if (isConnected_)
        return;
    
    INFO("connection is not established. attempting to connect...");
    getTransport()->connect(*face_);
    isConnected_ = true;
    INFO("connected to %s",face_->getHost());
}

//********************************************************************************
#pragma mark - overriden
void NdnWorker::dispatchData(shared_ptr<Data> &data)
{
    TRACE("should dispatch callback on main thread here");
    
    NS_DispatchToMainThread(new NdnWorkerDispatchDataTask(delegate_,data));
    
}

