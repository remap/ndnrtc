//
//  video-coder.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#include "video-coder.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - construction/destruction
NdnVideoCoder::NdnVideoCoder(NdnVideoCoderParams *params) : NdnRtcObject(params)
{
    
}
//********************************************************************************
#pragma mark - public

//********************************************************************************
#pragma mark - intefaces realization - IRawFrameConsumer
void NdnVideoCoder::onDeliverFrame(webrtc::I420VideoFrame &frame)
{
    TRACE("");
    
    if (frameConsumer_)
        frameConsumer_->onEncodedFrameDelivered();
}

//********************************************************************************
#pragma mark - private

