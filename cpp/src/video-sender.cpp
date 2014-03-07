//
//  video-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "video-sender.h"
#include "ndnlib.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnrtc;
using namespace webrtc;

//******************************************************************************
#pragma mark - overriden
int NdnVideoSender::init(const shared_ptr<Face> &face,
                         const shared_ptr<ndn::Transport> &transport)
{
    int res = MediaSender::init(face, transport);
    
    if (RESULT_FAIL(res))
        return res;
    
    shared_ptr<string>
    keyPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_, true);
    
    keyFramesPrefix_.reset(new Name(*keyPrefixString));
    keyFrameNo_ = 0; // key frames are starting numeration from 1
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - intefaces realization
void NdnVideoSender::onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage)
{
    // update packet rate meter
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    uint64_t publishingTime = NdnRtcUtils::microsecondTimestamp();
    
    // determine, whether we should publish under key or delta namespaces
    bool isKeyFrame = encodedImage._frameType == webrtc::kKeyFrame;
    
    shared_ptr<Name> framePrefix = (isKeyFrame)?keyFramesPrefix_:packetPrefix_;
    FrameNumber frameNo = (isKeyFrame)?keyFrameNo_:deltaFrameNo_;
    
    // copy frame into transport data object
    PacketData::PacketMetadata metadata;
    metadata.packetRate_ = getCurrentPacketRate();
    metadata.timestamp_ = encodedImage.capture_time_ms_;
    
    PrefixMetaInfo prefixMeta;
    prefixMeta.playbackNo_ = packetNo_;
    prefixMeta.keySequenceNo_ = keyFrameNo_;
    
    NdnFrameData frameData(encodedImage, metadata);
    
    if (RESULT_GOOD(publishPacket(frameData,
                                  framePrefix,
                                  frameNo,
                                  prefixMeta)))
    {
        publishingTime = NdnRtcUtils::microsecondTimestamp() - publishingTime;
        INFO("PUBLISHED: \t%d \t%d \t%ld \t%d \t%ld \t%.2f",
             getFrameNo(),
             isKeyFrame,
             publishingTime,
             frameData.getLength(),
             encodedImage.capture_time_ms_,
             getCurrentPacketRate());

        if (isKeyFrame)
            keyFrameNo_++;
        else
            deltaFrameNo_++;

        // increment packet number regardless of key/delta condition
        // in this case we can preserve that key frames can have numbers in
        // delta namespace as well
        packetNo_++;
    }
    else
    {
        notifyError(RESULT_ERR, "were not able to publish frame %d (KEY: %s)",
                    getFrameNo(), (isKeyFrame)?"YES":"NO");
    }
}
