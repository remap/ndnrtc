//
//  ndnrtc-object.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__ndnrtc_object__
#define __ndnrtc__ndnrtc_object__

#include <boost/enable_shared_from_this.hpp>
#include "simple-log.hpp"

namespace ndnrtc {
    class NdnRtcComponent : public ndnlog::new_api::ILoggingObject,
                            public boost::enable_shared_from_this<NdnRtcComponent>
    {
    public:
            // construction/desctruction
        NdnRtcComponent(){}
        virtual ~NdnRtcComponent(){}

            // ILoggingObject interface conformance
        virtual std::string
        getDescription() const;

        virtual bool
        isLoggingEnabled() const
        { return true; }
    };
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
