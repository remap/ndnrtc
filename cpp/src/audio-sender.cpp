//
//  audio-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "audio-sender.h"
#include "frame-buffer.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace boost;

//******************************************************************************
//******************************************************************************
#pragma mark - static
int NdnAudioSender::getStreamControlPrefix(const ParamsStruct &params,
                                           std::string &prefix)
{
    int res = RESULT_OK;
    shared_ptr<std::string> streamPrefix = NdnRtcNamespace::getStreamPrefix(params);
    
    std::string streamThread = NdnRtcUtils::toString("%d", params.streamsParams[0].startBitrate);
    
    // RTP and RTCP are published under the same prefix
#if 0
    string rtcpSuffix = "control";
#else
    std::string rtcpSuffix = "";
#endif
    prefix = *NdnRtcNamespace::buildPath(false,
                                         &(*streamPrefix),
                                         &streamThread,
                                         &rtcpSuffix,
                                         &NameComponents::NameComponentStreamFramesDelta,
                                         NULL);
    
    return res;
}

//******************************************************************************
#pragma mark - public
int NdnAudioSender::init(const shared_ptr<FaceProcessor>& faceProcessor,
                         const shared_ptr<KeyChain>& ndnKeyChain)
{
    int res = RESULT_OK;
    shared_ptr<std::string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_, 0);
    res = MediaSender::init(faceProcessor, ndnKeyChain, packetPrefix);
    
    if (RESULT_FAIL(res))
        return res;
    
    std::string prefix;
    
    res = NdnAudioSender::getStreamControlPrefix(params_, prefix);
    
    if (RESULT_GOOD(res))
        rtcpPacketPrefix_.reset(new Name(prefix.c_str()));
    
    return res;
}

int NdnAudioSender::publishPacket(PacketData &packetData,
                                  PrefixMetaInfo prefixMeta)
{
    shared_ptr<Name> packetPrefix(new Name(*packetPrefix_));
    packetPrefix->append(NdnRtcUtils::componentFromInt(packetNo_));
    NdnRtcNamespace::appendDataKind(packetPrefix, false);
    
    prefixMeta.totalSegmentsNum_ = Segmentizer::getSegmentsNum(packetData.getLength(),
                                                               segmentSize_);
    // no fec for audio
    prefixMeta.paritySegmentsNum_ = 0;
    prefixMeta.playbackNo_ = packetNo_;
    
    return MediaSender::publishPacket(packetData, packetPrefix, packetNo_,
                                      prefixMeta, NdnRtcUtils::unixTimestamp());
}

int NdnAudioSender::publishRTPAudioPacket(unsigned int len, unsigned char *data)
{
    // update packet rate meter
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    
    NdnAudioData::AudioPacket packet = {false, len, data};
    NdnAudioData adata(packet);

    publishPacket(adata);
    packetNo_++;
    
    return 0;
}

int NdnAudioSender::publishRTCPAudioPacket(unsigned int len, unsigned char *data)
{
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    
    NdnAudioData::AudioPacket packet = (NdnAudioData::AudioPacket){true, len, data};
    NdnAudioData adata(packet);
    
    publishPacket(adata);
    packetNo_++;
    
    return 0;
}
//******************************************************************************
#pragma mark - intefaces realization - <#interface#>

//******************************************************************************
#pragma mark - private

