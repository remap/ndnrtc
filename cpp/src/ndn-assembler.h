//
//  ndn-assembler.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__ndn_assembler__
#define __ndnrtc__ndn_assembler__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace new_api {
        using namespace ndn;
        using namespace ptr_lib;
        
        class IPacketAssembler {
        public:
            virtual ~IPacketAssembler(){}
            
            virtual ndn::OnData getOnDataHandler() = 0;
            virtual ndn::OnTimeout getOnTimeoutHandler() = 0;
        };
        
        class Assembler {
        public:

            ndn::OnData getOnDataHandler();
            ndn::OnTimeout getOnTimeoutHandler();

            
            static shared_ptr<Assembler> getSharedInstance();
            static ndn::OnData defaultOnDataHandler();
            static ndn::OnTimeout defaultOnTimeoutHandler();
        private:
            
        };
    }
}

#endif /* defined(__ndnrtc__ndn_assembler__) */
