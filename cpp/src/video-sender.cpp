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

NdnVideoSender::NdnVideoSender(const ParamsStruct& params):MediaSender(params)
{
    description_ = "vsender";
}

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
    keyFrameNo_ = -1;

    LogInfoC << "initialized" << endl;
    
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
    
    if (isKeyFrame)
        keyFrameNo_++;
    
    shared_ptr<Name> framePrefix = (isKeyFrame)?keyFramesPrefix_:packetPrefix_;
    FrameNumber frameNo = (isKeyFrame)?keyFrameNo_:deltaFrameNo_;
    
    // copy frame into transport data object
    PacketData::PacketMetadata metadata;
    metadata.packetRate_ = getCurrentPacketRate();
    metadata.timestamp_ = encodedImage.capture_time_ms_;
    
    PrefixMetaInfo prefixMeta;
    prefixMeta.playbackNo_ = packetNo_;
    prefixMeta.pairedSequenceNo_ = (isKeyFrame)?deltaFrameNo_:keyFrameNo_;
    
    NdnFrameData frameData(encodedImage, metadata);
    int nSegments = 0;
    
    if ((nSegments = publishPacket(frameData,
                                  framePrefix,
                                  frameNo,
                                  prefixMeta)) > 0)
    {
        publishingTime = NdnRtcUtils::microsecondTimestamp() - publishingTime;
//        INFO("PUBLISHED: \t%d \t%d \t%ld \t%d \t%ld \t%.2f",
//             getFrameNo(),
//             isKeyFrame,
//             publishingTime,
//             frameData.getLength(),
//             encodedImage.capture_time_ms_,
//             getCurrentPacketRate());

        LogStatC
        << "publish\t" << packetNo_ << "\t"
        << deltaFrameNo_ << "\t"
        << keyFrameNo_ << "\t"
        << (isKeyFrame?"K":"D") << "\t"
        << publishingTime << "\t"
        << frameData.getLength() << "\t"
        << encodedImage.capture_time_ms_ << "\t"
        << getCurrentPacketRate() << "\t"
        << nSegments << endl;
        
        if (!isKeyFrame)
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

void NdnVideoSender::onInterest(const shared_ptr<const Name>& prefix,
                             const shared_ptr<const Interest>& interest,
                             ndn::Transport& transport)
{
    const Name& name = interest->getName();
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(name);
    bool isKeyNamespace = NdnRtcNamespace::isKeyFramePrefix(name);
    
    if (packetNo > (isKeyNamespace)?keyFrameNo_:deltaFrameNo_)
    {
        addToPit(interest);
    }
    else
    {
        LogTraceC
        << "late interest " << name << endl;
    }
}