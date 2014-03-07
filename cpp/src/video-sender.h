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
            this->setLogger(new NdnLogger(NdnLoggerDetailLevelAll,
                                          "publish-vchannel-%s.log",
                                          params.producerId));
            isLoggerCreated_ = true;
        }
        ~NdnVideoSender()
        {
        }
        
        // overriden from base class
        int init(const shared_ptr<Face> &face,
                 const shared_ptr<ndn::Transport> &transport);
        
        unsigned long int getFrameNo() { return getPacketNo(); }
        
        // interface conformance
        void onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage);
        
    private:
        int keyFrameNo_ = 0, deltaFrameNo_ = 0;
        shared_ptr<Name> keyFramesPrefix_;
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
