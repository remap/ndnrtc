// 
//  params-validator.h
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <string>

#include "params.h"
#include "ndnrtc-common.h"
#include "ndnrtc-exception.h"

namespace ndnrtc {
    
    struct param_validation_exception : public ndnrtc_exception
    {
    public:
        param_validation_exception(const char* what_arg):ndnrtc_exception(what_arg){}
        param_validation_exception(const std::string& what_arg):ndnrtc_exception(what_arg){}
    };
    
    namespace new_api {
        class ParamsValidator
        {
        public:
            static const unsigned int MaxUserNameLen;
            static const unsigned int MaxStreamNameLen;
            static const unsigned int MaxThreadNameLen;
            
            static void validateParams(const MediaThreadParams& params);
//            static void validateParams(const AudioThreadParams& params);
            static void validateParams(const VideoThreadParams& params);
            static void validateParams(const VideoCoderParams& params);
            static void validateParams(const GeneralProducerParams& params);
            static void validateParams(const MediaStreamParams& params);
            static void validateParams(const GeneralConsumerParams& params);
            static void validateParams(const HeadlessModeParams& params);
            static void validateParams(const GeneralParams& params);
            static void validateParams(const AppParams& params);
            static void validateName(const std::string& name, const int maxLength);
            
        private:
        };
    }
};