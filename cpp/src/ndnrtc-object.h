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
#include "params.h"

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
            virtual ~Parameter(){ if (value_ != nullptr) free(value_); };
            
            // public attributes
            virtual void setTypeAndValue(const ParameterType type, const void *value);
            virtual ParameterType getType() const { return type_; };
            virtual void* getValue() const { return value_; };
            
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
        
        // static attributes
        // capturer param names
        static const std::string ParamNameDeviceId;
        static const std::string ParamNameWidth;
        static const std::string ParamNameHeight;
        static const std::string ParamNameFPS;
        
        // renderer param names
        static const std::string ParamNameWindowWidth;
        static const std::string ParamNameWindowHeight;
        
        // encoder parameters names
        static const std::string ParamNameFrameRate;
        static const std::string ParamNameStartBitRate;
        static const std::string ParamNameMaxBitRate;
        static const std::string ParamNameEncodeWidth;
        static const std::string ParamNameEncodeHeight;
        
        // producer parameters names
        static const std::string ParamNameStreamName;
        static const std::string ParamNameStreamThreadName;
        static const std::string ParamNameNdnHub;
        static const std::string ParamNameProducerId;
        static const std::string ParamNameSegmentSize;
        static const std::string ParamNameFrameFreshnessInterval;
        
        // consumer parameters names
        static const std::string ParamNameProducerRate;
        static const std::string ParamNameReceiverId;
        static const std::string ParamNameInterestTimeout;
        static const std::string ParamNameFrameBufferSize;
        static const std::string ParamNameFrameSlotSize;
        
        // network parameters names
        static const std::string ParamNameConnectHost;
        static const std::string ParamNameConnectPort;
        
        // public static goes here
        static shared_ptr<NdnParams> defaultParams();
        
        // public methods go here
        virtual int size(){ return propertiesMap_.size(); };
        virtual void addParams(const ndnrtc::NdnParams& params);
        virtual void resetParams(const NdnParams &params);
        virtual void setIntParam(const std::string &name, const int value) { setParam(name, Parameter(ParameterTypeInt, &value)); };
        virtual void setBoolParam(const std::string &name, const bool value) { setParam(name, Parameter(ParameterTypeBool, &value)); };
        virtual void setStringParam(const std::string &name, const std::string &value) { setParam(name, Parameter(ParameterTypeString, (char*)value.c_str())); };
        
        // prints out all the current params into a string
        virtual std::string description() const;
        
    protected:
        // protected methods go here
        virtual const std::map<std::string, Parameter>& map() const { return propertiesMap_; };
        virtual void setParam(const std::string &name, const Parameter &param);
        virtual int getParamAsInt(const std::string &paramName, int *param) const;
        virtual int getParamAsBool(const std::string &paramName, bool *param) const;
        virtual int getParamAsString(const std::string &paramName, char** param) const;
        
        virtual std::string getParamAsString(const std::string &paramName) const;
        virtual int getParamAsInt(const std::string &paramName) const;
        virtual bool getParamAsBool(const std::string &paramName) const;
        
    private:
        // attributes
        std::map<std::string, Parameter> propertiesMap_;
        
        // methods
        NdnParams::Parameter *getParam(const std::string &name) const;
    };
    
    class NdnRtcObject : public INdnRtcObjectObserver {
    public:
        // construction/desctruction
        NdnRtcObject(const ParamsStruct &params = DefaultParams);
        NdnRtcObject(const ParamsStruct &params, INdnRtcObjectObserver *observer);
        virtual ~NdnRtcObject();
        
        // public methods go here
        void setObserver(INdnRtcObjectObserver *observer) { observer_ = observer; }
        virtual void onErrorOccurred(const char *errorMessage);
        
    protected:
        // critical section for observer's callbacks
        webrtc::CriticalSectionWrapper &callbackSync_;
        INdnRtcObjectObserver *observer_ = nullptr;
        ParamsStruct params_;
        
        // protected methods go here
        int notifyError(const int ecode, const char *format, ...);
        int notifyErrorNoParams();
        int notifyErrorBadArg(const std::string &paramName);
        bool hasObserver() { return observer_ != nullptr; };
    };
}

#endif /* defined(__ndnrtc__ndnrtc_object__) */
