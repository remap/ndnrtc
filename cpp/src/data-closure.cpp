//
//  dataclosure.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/6/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "data-closure.h"

using namespace ndn;
using namespace ndnrtc;
using namespace std;

UpcallResult DataClosure::upcall(UpcallKind kind, UpcallInfo &upcallInfo)
{
    if (kind == UPCALL_DATA || kind == UPCALL_DATA_UNVERIFIED) {
        gotContent_ = true;
        dispatchData(upcallInfo.getData());
    }
    else
        ;
    
    return CLOSURE_RESULT_OK;
}