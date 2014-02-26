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
using namespace std;

//******************************************************************************
//******************************************************************************
#pragma mark - static
int NdnAudioSender::getStreamControlPrefix(const ParamsStruct &params,
                                           string &prefix)
{
    int res = RESULT_OK;
    shared_ptr<string> streamPrefix = NdnRtcNamespace::getStreamPrefix(params);
    
    string streamThread = ParamsStruct::validate(params.streamThread,
                                                 DefaultParamsAudio.streamThread,
                                                 res);
    // RTP and RTCP are published under the same prefix
#if 0
    string rtcpSuffix = "control";
#else
    string rtcpSuffix = NdnRtcNamespace::NameComponentStreamFrames;
#endif
    prefix = *NdnRtcNamespace::buildPath(false,
                                         &(*streamPrefix),
                                         &streamThread,
                                         &rtcpSuffix,
                                         &NdnRtcNamespace::NameComponentStreamFramesDelta,
                                         NULL);
    
    return res;
}

//******************************************************************************
#pragma mark - public
int NdnAudioSender::init(const shared_ptr<Face> &face,
                         const shared_ptr<ndn::Transport> &transport)
{
    int res = RESULT_OK;
    
    res = MediaSender::init(face, transport);
    
    if (RESULT_FAIL(res))
        return res;
    
    string prefix;
    
    res = NdnAudioSender::getStreamControlPrefix(params_, prefix);
    
    if (RESULT_GOOD(res))
        rtcpPacketPrefix_.reset(new Name(prefix.c_str()));
    
    return res;
}

int NdnAudioSender::publishRTPAudioPacket(unsigned int len, unsigned char *data)
{
    // update packet rate meter
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    
    NdnAudioData::AudioPacket packet {false, NdnRtcUtils::millisecondTimestamp(),
        len, data};
    PacketData::PacketMetadata metadata = {getCurrentPacketRate()};
    NdnAudioData adata(packet, metadata);
    
    publishPacket(adata.getLength(), adata.getData());
    packetNo_++;
    
    return 0;
}

int NdnAudioSender::publishRTCPAudioPacket(unsigned int len, unsigned char *data)
{
    NdnAudioData::AudioPacket packet {true, NdnRtcUtils::millisecondTimestamp(),
        len, data};
    NdnAudioData adata(packet);
    
    publishPacket(adata.getLength(), adata.getData());
    packetNo_++;
    
    return 0;
}
//******************************************************************************
#pragma mark - intefaces realization - <#interface#>

//******************************************************************************
#pragma mark - private

