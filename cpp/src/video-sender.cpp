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
    string frameType = NdnRtcUtils::stringFromFrameType(encodedImage._frameType);
    
    // send frame over NDN
    TRACE("sending frame #%010lld (%s)", getFrameNo(),
          frameType.c_str());
#ifdef USE_FRAME_LOGGER
    frameLogger_->logString(frameType.c_str());
#endif
    
    // 0. copy frame into transport data object
    NdnFrameData frameData(encodedImage);
    if (RESULT_GOOD(publishPacket(frameData.getLength(),
                  const_cast<unsigned char*>(frameData.getData()))))
        packetNo_++;
}
