//
//  ndNrtc.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 7/29/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "ndNrtc.h"

#include "ndnrtc-common.h"

// internal headers
#include "data-closure.h"
#include "ndnworker.h"

#define NDN_BUFFER_SIZE 8000

using namespace std;
using namespace ndn;
using namespace ptr_lib;
using namespace ndnrtc;

NS_IMPL_ISUPPORTS1(ndNrtc, INrtc)

//********************************************************************************
#pragma mark module loading
__attribute__((constructor))
static void initializer(int argc, char** argv, char** envp) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        INFO("module loaded");
    }
}

__attribute__((destructor))
static void destructor(){
    INFO("module unloaded");
}

//********************************************************************************
#pragma mark - creation
ndNrtc::ndNrtc()
{
    INFO("constructor called");
//    currentWorker_ = shared_ptr<NdnWorker>(new NdnWorker("E.hub.ndn.ucla.edu", 9695, this));
    // connect to local ndnd
    currentWorker_ = shared_ptr<NdnWorker>(new NdnWorker("localhost", 9695, this));
};
ndNrtc::~ndNrtc(){
    INFO("destructor called");
};

//********************************************************************************
#pragma mark - idl interface implementation
NS_IMETHODIMP ndNrtc::Test(int32_t a, int32_t b, int32_t *_retval)
{
	*_retval = a+b;
    return NS_OK;
}

NS_IMETHODIMP ndNrtc::ExpressInterest(const char *interest, INrtcDataCallback *onDataCallback)
{
    dataCallback_ = onDataCallback;
    
    try {
        TRACE("calling expressInterestAsync");
        currentWorker_->expressInterestAsync(interest);
    }
    catch (std::exception &e)
    {
        ERROR("exception: %s", e.what());
    }
    
    return NS_OK;
}

NS_IMETHODIMP ndNrtc::PublishData(const char *prefix, const char *data)
{
    try {
        currentWorker_->publishData(prefix,(const unsigned char*)data,strlen(data));
    }
    catch (std::exception &e)
    {
        ERROR("exception: %s", e.what());
    }

    return NS_OK;
}

//********************************************************************************
#pragma mark - private
void ndNrtc::onDataReceived(shared_ptr<Data> &data)
{
    static char dataBuffer[NDN_BUFFER_SIZE];
    
    memset(dataBuffer, 0, NDN_BUFFER_SIZE);
    memcpy(dataBuffer, (const char*)&data->getContent()[0], data->getContent().size());
    
    TRACE("got content with name %s", data->getName().to_uri().c_str());
    
    if (dataCallback_)
        dataCallback_->OnNewData(data->getContent().size(), dataBuffer);
    
    for (unsigned int i = 0; i < data->getContent().size(); ++i)
        cout << data->getContent()[i];
    
    cout << endl;
}
