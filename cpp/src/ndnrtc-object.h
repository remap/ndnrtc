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
            Parameter():type_(ParameterTypeUndefined){ value_ = nullptr; };
            Parameter(const ParameterType type, const void *value);
            Parameter(const Parameter &p) : Parameter(p.type_, p.value_) {  };
            Parameter(Parameter &p) : Parameter(p.type_, p.value_) {  };
            ~Parameter(){ if (value_ != nullptr) free(value_); };
            
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
        NdnParams(const NdnParams &params) { addParams(params); };
        virtual ~NdnParams() {};
        
        // public static goes here
        static NdnParams* defaultParams(){
            return new NdnParams();
        };
        
        // public methods go here
        virtual int size(){ return propertiesMap_.size(); };
        void addParams(const ndnrtc::NdnParams& params);
        void resetParams(const NdnParams &params);
        void setIntParam(const std::string &name, const int value) { setParam(name, Parameter(ParameterTypeInt, &value)); };
        void setBoolParam(const std::string &name, const bool value) { setParam(name, Parameter(ParameterTypeBool, &value)); };
        void setStringParam(const std::string &name, const std::string &value) { setParam(name, Parameter(ParameterTypeString, (char*)value.c_str())); };

    protected:
        // protected methods go here
        const std::map<std::string, Parameter>& map() const { return propertiesMap_; };
        void setParam(const std::string &name, const Parameter &param);
        int getParamAsInt(const std::string &paramName, int *param) const;
        int getParamAsBool(const std::string &paramName, bool *param) const;
        int getParamAsString(const std::string &paramName, char** param) const;
        
    private:
        // attributes
        std::map<std::string, Parameter> propertiesMap_;
        
        // methods
        NdnParams::Parameter *getParam(const std::string &name) const;
    };
    
    class NdnRtcObject : public INdnRtcObjectObserver {
    public:
        // construction/desctruction
        NdnRtcObject();
        NdnRtcObject(const NdnParams *params);
        NdnRtcObject(const NdnParams *params, INdnRtcObjectObserver *observer);
        virtual ~NdnRtcObject();
        
        // public methods go here
        void setObserver(INdnRtcObjectObserver *observer) { observer_ = observer; }
        virtual void onErrorOccurred(const char *errorMessage);
        
    protected:
        // protected attributes go here
        INdnRtcObjectObserver *observer_ = nullptr;
        NdnParams *params_ = nullptr;
        
        // protected methods go here
        int notifyError(const int ecode, const char *format, ...);
        int notifyErrorNoParams();
        int notifyErrorBadArg(const std::string &paramName);
        bool hasObserver() { return observer_ != nullptr; };
        bool hasParams() { return params_ != nullptr; }; // && params_->size() != 0; };
    };
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
