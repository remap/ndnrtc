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
     * This class is a sink for encoded frames and it publishes them on ndn 
     * network under the prefix, determined by the parameters.
     */
    class NdnVideoSender : public MediaSender, public IEncodedFrameConsumer
    {
    public:
        NdnVideoSender(const ParamsStruct &params);
        ~NdnVideoSender(){}
     
        static const double ParityRatio;
        
        // overriden from base class
        int init(const shared_ptr<FaceProcessor>& faceProcessor,
                 const shared_ptr<KeyChain>& ndnKeyChain);
        
        unsigned long int getFrameNo() { return getPacketNo(); }
        
        // interface conformance
        void onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                     double captureTimestamp);
        
    private:
        int keyFrameNo_ = 0, deltaFrameNo_ = 0;
        shared_ptr<Name> keyFramesPrefix_;
        
        void onInterest(const shared_ptr<const Name>& prefix,
                        const shared_ptr<const Interest>& interest,
                        ndn::Transport& transport);
        
        int
        publishParityData(PacketNumber frameNo,
                          const PacketData &packetData,
                          unsigned int nSegments,
                          const shared_ptr<Name>& framePrefix,
                          const PrefixMetaInfo& prefixMeta);
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
