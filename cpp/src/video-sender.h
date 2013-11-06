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

//#define USE_FRAME_LOGGER

#include "ndnrtc-common.h"
#include "ndnrtc-namespace.h"
#include "video-coder.h"
#include "frame-buffer.h"
#include "media-sender.h"

namespace ndnrtc
{
    class INdnVideoSenderDelegate;
    
    /**
     * This class is a sink for encoded frames and it publishes them on ndn 
     * network under the prefix, determined by the parameters.
     */
    class NdnVideoSender : public MediaSender, public IEncodedFrameConsumer
    {
    public:
        NdnVideoSender(const ParamsStruct &params):MediaSender(params)
        {
#ifdef USE_FRAME_LOGGER
            frameLogger_ = new NdnLogger("frames.log", NdnLoggerDetailLevelDefault);
#endif
        }
        ~NdnVideoSender()
        {
#ifdef USE_FRAME_LOGGER
            delete frameLogger_;
#endif
        };
        
        unsigned long int getFrameNo() { return getPacketNo(); }
        
        // interface conformance
        void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage);
        
    private:
#ifdef USE_FRAME_LOGGER
        NdnLogger *frameLogger_;
#endif
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
