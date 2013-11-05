//
//  video-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

//#define NDN_LOGGING
//#define NDN_INFO
//#define NDN_WARN
//#define NDN_ERROR

//#define NDN_DETAILED
//#define NDN_TRACE
//#define NDN_DEBUG

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
    // send frame over NDN
    TRACE("sending frame #%010lld", getFrameNo());
    
    // 0. copy frame into transport data object
    NdnFrameData frameData(encodedImage);
    publishPacket(frameData.getLength(),
                  const_cast<unsigned char*>(frameData.getData()));
    packetNo_++;
}
