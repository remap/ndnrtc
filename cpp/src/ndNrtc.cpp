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
        cout << "module loaded" << endl;        
    }
}

__attribute__((destructor))
static void destructor(){
    cout << "module unloaded" << endl;
}

//********************************************************************************
#pragma mark - creation
ndNrtc::ndNrtc()
{
    std::cout<<"Nrtc: constructor called"<<std::endl;
    currentWorker_ = shared_ptr<NdnWorker>(new NdnWorker("E.hub.ndn.ucla.edu", 9695, this));
};
ndNrtc::~ndNrtc(){
    std::cout<<"Nrtc: destructor called"<<std::endl;
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
        cout<<"calling expressInterestAsync"<<endl;
        currentWorker_->expressInterestAsync(interest);
    }
    catch (std::exception &e)
    {
        cout << "exception: " << e.what() << endl;
    }
    
    return NS_OK;
}

//********************************************************************************
#pragma mark - private
void ndNrtc::onDataReceived(shared_ptr<Data> &data)
{
    cout << "got content with name " << data->getName().to_uri() << endl;
    
    if (dataCallback_)
        dataCallback_->OnNewData(data->getContent().size(), (const char*)&data->getContent()[0]);
    
    for (unsigned int i = 0; i < data->getContent().size(); ++i)
        cout << data->getContent()[i];
    
    cout << endl;
}
