//  Copyright (C) 2013 Regents of the University of California
//  Author(s): Peter Gusev <gpeetonn@gmail.com>
//  See the LICENSE file for licensing information.
//

// Created: 7/29/2013
// Last updated: 7/29/2013

#include "ndINrtc.h"

#include "ndnrtc-common.h"

#include "data-closure.h"
#include "ndnworker.h"
#include "camera-capturer.h"
#include "renderer.h"

// These macros are used in ndnNrtModule.cpp
#define NRTC_CLASSNAME 	"NDN WebRTC Main Class"
#define NRTC_CONTRACTID "@named-data.net/ndnrtc;1"
// "CD232E0F-A777-41A3-BB19-CF415B98088E"
#define NRTC_CID \
  {0xcd232e0f, 0xa777, 0x41a3, \
    { 0xbb, 0x19, 0xcf, 0x41, 0x5b, 0x98, 0x08, 0x8e }}

using namespace ndnrtc;

/** 
 * Class description goes here
 */
class ndNrtc : public INrtc
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_INRTC

    ndNrtc();
    ~ndNrtc();

private:
    // should be shared_ptr, cause it's implementing
    // emable_shared_from_this internally
//    shared_ptr<ndnrtc::NdnWorker> currentWorker_;
    
    CameraCapturer *cameraCapturer_;
    NdnRenderer *localRenderer_, *remoteRenderer_;
    
protected:
  /* additional members */
};
