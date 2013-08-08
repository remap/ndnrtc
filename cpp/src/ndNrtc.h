//
//  ndNrtc.h
//  ndnrtc
//
//  Created by Peter Gusev on 7/29/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "ndINrtc.h"

#include "ndnrtc-common.h"
#include "data-closure.h"
#include "ndnworker.h"

// These macros are used in ndnNrtModule.cpp
#define NRTC_CLASSNAME 	"NDN WebRTC Main Class"
#define NRTC_CONTRACTID "@named-data.net/ndnrtc;1"
// "CD232E0F-A777-41A3-BB19-CF415B98088E"
#define NRTC_CID \
  {0xcd232e0f, 0xa777, 0x41a3, \
    { 0xbb, 0x19, 0xcf, 0x41, 0x5b, 0x98, 0x08, 0x8e }}

/** 
 * Class description goes here
 */
class ndNrtc : public INrtc, public ndnrtc::INdnWorkerDelegate
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INRTC

    ndNrtc();
    ~ndNrtc();

private:
    // should be shared_ptr, cause it's implementing
    // emable_shared_from_this internally
    shared_ptr<ndnrtc::NdnWorker> currentWorker_;
    nsCOMPtr<INrtcDataCallback> dataCallback_;

    void onDataReceived(shared_ptr<ndn::Data> &data);
    
protected:
  /* additional members */
};
