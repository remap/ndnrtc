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

#include "ndnrtc-common.h"

namespace ndnrtc {
    class INdnRtcObjectObserver {
    public:
        // public methods go here
        virtual void onErrorOccurred(const char *errorMessage) = 0;
    };
    
    class NdnParams {
    public:
        enum ParameterType {
            ParameterTypeString = 0,
            ParameterTypeInt = 1,
            ParameterTypeBool = 2
        };
        
        class Parameter
        {
        public:
            Parameter() {};
            Parameter(ParameterType type, void *value);
//            Parameter(Parameter *p) : Parameter(p->type_, p->value_) {};
//            Parameter(Parameter &p) : Parameter(p.type_, p.value_) {};
            ~Parameter(){ free(value_); };
            // public attributes
            ParameterType type_;
            void *value_;
        };
        
        // construction/desctruction
        NdnParams() {};
        NdnParams(NdnParams &params) { addParams(params); };
        ~NdnParams() {};
        
        // public static goes here
        static NdnParams* defaultParams(){
            return new NdnParams();
        };
        
        // public methods go here
        std::map<std::string, Parameter> map() { return propertiesMap_; };
        void addParams(NdnParams &params);
        void resetParams(NdnParams &params);
        void setIntParam(const std::string &name, int value) { setParam(name, Parameter(ParameterTypeInt, &value)); };
        void setBoolParam(const std::string &name, bool value) { setParam(name, Parameter(ParameterTypeBool, &value)); };
        void setStringParam(const std::string &name, std::string &value) { setParam(name, Parameter(ParameterTypeBool, (char*)value.c_str())); };
    protected:
        // protected methods go here
        void setParam(const std::string &name, const Parameter &param) { propertiesMap_[name] = param; };
        int getParamAsInt(const std::string &paramName, int *param);
        int getParamAsBool(const std::string &paramName, bool *param);
        int getParamAsString(const std::string &paramName, char** param);
        
    private:
        std::map<std::string, Parameter> propertiesMap_;
    };
    
    class NdnRtcObject : public INdnRtcObjectObserver {
    public:
        // construction/desctruction
        NdnRtcObject();
        NdnRtcObject(NdnParams *params);
        NdnRtcObject(NdnParams *params, INdnRtcObjectObserver *observer);
        ~NdnRtcObject();
        
        // public methods go here
        void setObserver(INdnRtcObjectObserver *observer) { observer_ = observer; }
        virtual void onErrorOccurred(const char *errorMessage);
        
    protected:
        // protected attributes go here
        INdnRtcObjectObserver *observer_;
        NdnParams *params_;
        
        // protected methods go here
        int notifyError(const int ecode, const char *format, ...);
        int notifyErrorNoParams();
        int notifyErrorBadArg(const std::string &paramName);
        bool hasObserver() { return observer_ != NULL; };
        bool hasParams() { return params_ != NULL; };
    };
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
