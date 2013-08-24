//
//  dataclosure.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/6/13
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