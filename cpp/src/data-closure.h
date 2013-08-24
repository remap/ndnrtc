//
//  dataclosure.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/6/13
//

#ifndef __ndnrtc__dataclosure__
#define __ndnrtc__dataclosure__

#include <iostream>

#include "ndnrtc-common.h"

namespace ndnrtc
{
    class DataClosure : public Closure {
    public:
        DataClosure():
        gotContent_(false){}
        
        virtual void dispatchData(shared_ptr<Data> &data){/*empty*/};

        bool gotContent_;
    private:
        UpcallResult upcall(UpcallKind kind, UpcallInfo &upcallInfo);
    };
}

#endif /* defined(__ndnrtc__dataclosure__) */
