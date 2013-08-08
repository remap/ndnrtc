//
//  dataclosure.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/6/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
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
