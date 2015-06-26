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
    // in order to avoid situations when interest arrives simultaneously
    // with the data being added to the PIT/cache, we synchronize with
    // face on this level
    faceProcessor_->getFaceWrapper()->synchronizeStart();
    
    NdnAudioData::AudioPacket packet = {false, len, data};
    
    if ((adata_.getLength() + packet.getLength()) > segSizeNoHeader_)
    {
        // update packet rate meter
        NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
        
        // publish adata and flush
        publishPacket(adata_);
        adata_.clear();
        packetNo_++;
    }

    adata_.addPacket(packet);
    
    faceProcessor_->getFaceWrapper()->synchronizeStop();
    return 0;
}

int AudioThread::publishRTCPAudioPacket(unsigned int len, unsigned char *data)
{
    // in order to avoid situations when interest arrives simultaneously
    // with the data being added to the PIT/cache, we synchronize with
    // face on this level
    faceProcessor_->getFaceWrapper()->synchronizeStart();
    
    NdnAudioData::AudioPacket packet = (NdnAudioData::AudioPacket){true, len, data};
    
    if ((adata_.getLength() + packet.getLength()) > segSizeNoHeader_)
    {
        NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
        // publish adata and flush
        publishPacket(adata_);
        adata_.clear();
        packetNo_++;
    }

    adata_.addPacket(packet);
    
    faceProcessor_->getFaceWrapper()->synchronizeStop();
    return 0;
}
