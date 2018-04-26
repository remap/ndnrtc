// 
// validator.hpp
//
//  Created by Peter Gusev on 05 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __validator_h__
#define __validator_h__

#include <boost/shared_ptr.hpp>
#include "simple-log.hpp"

namespace ndn {
    class KeyChain;
    class Data;
};

namespace ndnrtc {
    namespace new_api {
        class FrameBuffer;
    };
    class ISlotBuffer;

    class ValidationErrorInfo {
    public:
        ValidationErrorInfo(const boost::shared_ptr<const ndn::Data>& data, 
                            const std::string reason = ""):
            failedData_(data), reason_(reason){}
    
        boost::shared_ptr<const ndn::Data> getData() const 
        { return failedData_; }
        const std::string& getReason() const 
        { return reason_; }
    
    private:
        std::string reason_;
        boost::shared_ptr<const ndn::Data> failedData_;
    };

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
        void onVerifyFailure(const boost::shared_ptr<ndn::Data>& data, 
            const std::string& reason);
        
        void markBufferData(const boost::shared_ptr<ndn::Data>& data, bool verified);
	};
};

#endif