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
    class NdnVideoSender : public MediaSender, public IRawFrameConsumer,
    public IEncodedFrameConsumer
    {
    public:
        NdnVideoSender(const ParamsStruct &params,
                       const CodecParams &codecParams);
        ~NdnVideoSender(){}
     
        static const double ParityRatio;
        
        // overriden from base class
        int init(const boost::shared_ptr<FaceProcessor>& faceProcessor,
                 const boost::shared_ptr<KeyChain>& ndnKeyChain);
        
        unsigned long int getFrameNo() { return getPacketNo(); }
        unsigned int getEncoderDroppedNum()
        { return coder_->getDroppedFramesNum(); }
        
        // interface conformance
        void onDeliverFrame(webrtc::I420VideoFrame &frame,
                            double unixTimeStamp);
        
        void setLogger(ndnlog::new_api::Logger *logger);
        
    private:
        CodecParams codecParams_;
        int keyFrameNo_ = 0, deltaFrameNo_ = 0;
        boost::shared_ptr<Name> keyFramesPrefix_;
        boost::shared_ptr<NdnVideoCoder> coder_;
        
        void onInterest(const boost::shared_ptr<const Name>& prefix,
                        const boost::shared_ptr<const Interest>& interest,
                        ndn::Transport& transport);
        
        int
        publishParityData(PacketNumber frameNo,
                          const PacketData &packetData,
                          unsigned int nSegments,
                          const boost::shared_ptr<Name>& framePrefix,
                          const PrefixMetaInfo& prefixMeta);
        
        // interface conformance
        void onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                     double captureTimestamp);
    };
    
    class INdnVideoSenderDelegate : public INdnRtcObjectObserver {
    public:
    };
}

#endif /* defined(__ndnrtc__video_sender__) */
