//
//  audio-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "audio-thread.h"
#include "frame-buffer.h"

using namespace ndnrtc::new_api;
using namespace webrtc;
using namespace boost;

//******************************************************************************
#pragma mark - public
int AudioThread::init(const AudioThreadSettings& settings)
{
    int res = RESULT_OK;
    res = MediaThread::init(settings);
    
    if (RESULT_FAIL(res))
        return res;
    
    rtpPacketPrefix_ = settings_.prefix_;
    rtcpPacketPrefix_ = settings_.prefix_;
    
    return res;
}

int AudioThread::publishPacket(PacketData &packetData,
                               PrefixMetaInfo prefixMeta)
{
    Name packetPrefix(rtpPacketPrefix_);
    packetPrefix.append(NdnRtcUtils::componentFromInt(packetNo_));
    NdnRtcNamespace::appendDataKind(packetPrefix, false);
    
    prefixMeta.totalSegmentsNum_ = Segmentizer::getSegmentsNum(packetData.getLength(),
                                                               segSizeNoHeader_);
    // no fec for audio
    prefixMeta.paritySegmentsNum_ = 0;
    prefixMeta.playbackNo_ = packetNo_;
    
    return MediaThread::publishPacket(packetData, packetPrefix, packetNo_,
                                      prefixMeta, NdnRtcUtils::unixTimestamp());
}

int AudioThread::publishRTPAudioPacket(unsigned int len, unsigned char *data)
{
    // update packet rate meter
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    
    NdnAudioData::AudioPacket packet = {false, len, data};
    NdnAudioData adata(packet);
    
    publishPacket(adata);
    packetNo_++;
    
    return 0;
}

int AudioThread::publishRTCPAudioPacket(unsigned int len, unsigned char *data)
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

