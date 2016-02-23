//
//  audio-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "ndnrtc-namespace.h"
#include "audio-thread.h"
#include "segmentizer.h"

using namespace ndnrtc::new_api;
using namespace webrtc;
using namespace boost;

//******************************************************************************
#pragma mark - public
int AudioThread::init(const AudioThreadSettings& settings)
{
    settings_ = new AudioThreadSettings();
    *settings_ = settings;
    
    int res = MediaThread::init(settings);
    
    if (RESULT_FAIL(res))
        return res;
    
    rtpPacketPrefix_ = Name(threadPrefix_);
    rtpPacketPrefix_.append(Name(NameComponents::NameComponentStreamFramesDelta));
    rtcpPacketPrefix_ = rtpPacketPrefix_;
    
    return res;
}

void AudioThread::onDeliverRtpFrame(unsigned int len, unsigned char *data)
{
    processAudioPacket({false, len, data});
}

void AudioThread::onDeliverRtcpFrame(unsigned int len, unsigned char *data)
{
    processAudioPacket({true, len, data});
}

int AudioThread::processAudioPacket(NdnAudioData::AudioPacket packet)
{
    if ((adata_.getLength() + packet.getLength()) > segSizeNoHeader_)
    {
        // update packet rate meter
        NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
        
        int nseg = publishPacket((PacketData&)adata_);
        packetNo_++;
        adata_.clear();
    }
    
    adata_.addPacket(packet);
    return 0;
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
