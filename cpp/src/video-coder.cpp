//
//  video-coder.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/21/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
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

