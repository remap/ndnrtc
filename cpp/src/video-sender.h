//
//  video-sender.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__video_sender__
#define __ndnrtc__video_sender__

#include "ndnrtc-common.h"
#include "ndnrtc-namespace.h"
#include "video-coder.h"
#include "frame-buffer.h"
#include "media-sender.h"

namespace ndnrtc
{
    class INdnVideoSenderDelegate;
    
    /**
     *
     */
    class NdnVideoSender : public MediaSender, public IEncodedFrameConsumer
    {
    public:
        static shared_ptr<MediaSenderParams> defaultParams()
        {
            shared_ptr<MediaSenderParams> p = MediaSenderParams::defaultParams();
            
            p->setStringParam(NdnParams::ParamNameStreamName,
                              string("video0"));
            p->setStringParam(NdnParams::ParamNameStreamThreadName,
                              string("vp8"));
            
            return p;
        }
        
        // construction/desctruction
        NdnVideoSender(const NdnParams * params):MediaSender(params){}
        ~NdnVideoSender(){};
        
        // public methods go here
        unsigned long int getFrameNo() { return getPacketNo(); };
        
        // interface conformance
        void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage);
        
    private:
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
