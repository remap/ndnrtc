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
            ParameterTypeUndefined = 0,
            ParameterTypeString = 1,
            ParameterTypeInt = 2,
            ParameterTypeBool = 3
        };
        
        class Parameter
        {
            friend class NdnParams;
        public:
            Parameter():type_(ParameterTypeUndefined){ value_ = NULL; TRACE("default");};
            Parameter(const ParameterType type, const void *value);
            Parameter(const Parameter &p) : Parameter(p.type_, p.value_) { TRACE("copy 1"); };
            Parameter(Parameter &p) : Parameter(p.type_, p.value_) { TRACE("copy 2"); };
            ~Parameter(){ if (value_ != NULL) free(value_); TRACE("dtor"); };
            
            // public attributes
            void setTypeAndValue(const ParameterType type, const void *value);
            ParameterType getType() const { return type_; };
            void* getValue() const { return value_; };
            
        private:
            // static methods
            static void *copiedValue(const ParameterType type, const void *value);
            static int valueByteSize(const ParameterType type, const void *value);
            
            // attributes
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
        int size(){ return propertiesMap_.size(); };
        void addParams(const ndnrtc::NdnParams& params);
        void resetParams(const NdnParams &params);
        void setIntParam(const std::string &name, const int value) { setParam(name, Parameter(ParameterTypeInt, &value)); };
        void setBoolParam(const std::string &name, const bool value) { setParam(name, Parameter(ParameterTypeBool, &value)); };
        void setStringParam(const std::string &name, const std::string &value) { setParam(name, Parameter(ParameterTypeString, (char*)value.c_str())); };

    protected:
        // protected methods go here
        const std::map<std::string, Parameter>& map() const { return propertiesMap_; };
        void setParam(const std::string &name, const Parameter &param);
        int getParamAsInt(const std::string &paramName, int *param);
        int getParamAsBool(const std::string &paramName, bool *param);
        int getParamAsString(const std::string &paramName, char** param);
        
    private:
        // attributes
        std::map<std::string, Parameter> propertiesMap_;
        
        // methods
        NdnParams::Parameter *getParam(const std::string &name);
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
