//
//  video-coder.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_coder__
#define __ndnrtc__video_coder__

#include "ndnrtc-common.h"
#include "camera-capturer.h"

namespace ndnrtc {
    class IEncodedFrameConsumer;
    
    class NdnVideoCoderParams : public NdnParams
    {
    public:
        // construction/desctruction
        NdnVideoCoderParams(){};
        ~NdnVideoCoderParams(){};
        
        // parameters names
        static const std::string ParamNameFrameRate;
        static const std::string ParamNameStartBitRate;
        static const std::string ParamNameMaxBitRate;
        static const std::string ParamNameWidth;
        static const std::string ParamNameHeight;
        
        // static public
        static NdnVideoCoderParams *defaultParams()
        {
            NdnVideoCoderParams *p = new NdnVideoCoderParams();
            
            p->setIntParam(ParamNameFrameRate, 30);
            p->setIntParam(ParamNameStartBitRate, 300);
            p->setIntParam(ParamNameMaxBitRate, 4000);
            p->setIntParam(ParamNameWidth, 640);
            p->setIntParam(ParamNameHeight, 480);
            
            return p;
        }
        
        // public methods
        int getFrameRate(int *frameRate) const { return getParamAsInt(ParamNameFrameRate, frameRate); };
        int getStartBitRate(int *startBitRate) const { return getParamAsInt(ParamNameStartBitRate, startBitRate); };
        int getMaxBitRate(int *maxBitRate) const { return getParamAsInt(ParamNameMaxBitRate, maxBitRate); };
        int getWidth(int *width) const { return getParamAsInt(ParamNameWidth, width); };
        int getHeight(int *height) const { return getParamAsInt(ParamNameHeight, height); };
    };
    
    class NdnVideoCoder : public NdnRtcObject, public IRawFrameConsumer, public webrtc::EncodedImageCallback 
    {
    public:
        // construction/desctruction
        NdnVideoCoder(const NdnParams *params);
        ~NdnVideoCoder() { TRACE("coder"); };
        
        // public methods go here
        void setFrameConsumer(IEncodedFrameConsumer *frameConsumer){ frameConsumer_ = frameConsumer; };
        int init();
        
        // interface conformance - webrtc::EncodedImageCallback
        int32_t Encoded(webrtc::EncodedImage& encodedImage,
                        const webrtc::CodecSpecificInfo* codecSpecificInfo = NULL,
                        const webrtc::RTPFragmentationHeader* fragmentation = NULL);
        
        // interface conformance - ndnrtc::IRawFrameConsumer
        void onDeliverFrame(webrtc::I420VideoFrame &frame);
    private:
        // private attributes go here
        IEncodedFrameConsumer *frameConsumer_ = nullptr;
        webrtc::VideoCodec codec_;
        shared_ptr<webrtc::VideoEncoder> encoder_;
        
        // private methods go here
        NdnVideoCoderParams *getParams() { return static_cast<NdnVideoCoderParams*>(params_); };
    };
    
    class IEncodedFrameConsumer
    {
    public:
        virtual void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage) = 0;
    };
}

#endif /* defined(__ndnrtc__video_coder__) */
