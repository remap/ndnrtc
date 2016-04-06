// 
// validator.h
//
//  Created by Peter Gusev on 05 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __validator_h__
#define __validator_h__

#include <boost/shared_ptr.hpp>
#include "simple-log.h"

namespace ndn {
    class KeyChain;
    class Data;
};

namespace ndnrtc {
    namespace new_api {
        class FrameBuffer;
    };
    class ISlotBuffer;

    template<typename KeyChainType>
    class DataValidator : public ndnlog::new_api::ILoggingObject
    {
    public:
        DataValidator(KeyChainType* keyChain, ISlotBuffer* buffer);
        ~DataValidator();
        
        void validate(const boost::shared_ptr<ndn::Data>& data);
        
    private:
        KeyChainType* keyChain_;
        ISlotBuffer* buffer_;

        void onVerifySuccess(const boost::shared_ptr<ndn::Data>& data);
        void onVerifyFailure(const boost::shared_ptr<ndn::Data>& data);
        
        void markBufferData(const boost::shared_ptr<ndn::Data>& data, bool verified);
	};
};

#endif