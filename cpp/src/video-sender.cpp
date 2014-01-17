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
#pragma mark - intefaces realization
void NdnVideoSender::onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
{
    // update packet rate meter
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    
    // send frame over NDN
    uint64_t publishingTime = NdnRtcUtils::microsecondTimestamp();
    
    // 0. copy frame into transport data object
    PacketData::PacketMetadata metadata = {getCurrentPacketRate()};
    NdnFrameData frameData(encodedImage, metadata);
    
    if (RESULT_GOOD(publishPacket(frameData.getLength(),
                                  const_cast<unsigned char*>(frameData.getData()))))
    {
        publishingTime = NdnRtcUtils::microsecondTimestamp() - publishingTime;
        INFO("PUBLISHED: \t%d \t%d \t%ld \t%d \t%ld \t%.2f",
             getFrameNo(),
             (encodedImage._frameType == webrtc::kKeyFrame),
             publishingTime,
             frameData.getLength(),
             encodedImage.capture_time_ms_,
             getCurrentPacketRate());
        packetNo_++;
    }
    else
    {
        notifyError(RESULT_ERR, "were not able to publish frame %d", getFrameNo());
    }
}
