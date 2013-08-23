//
//  video-sender.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/21/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "video-sender.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - construction/destruction

//********************************************************************************
#pragma mark - public
void NdnVideoSender::registerConference(ndnrtc::VideoSenderParams &params)
{
    params_.resetParams(params);
    TRACE("");
}

void NdnVideoSender::fetchParticipantsList()
{
    TRACE("");
}

//********************************************************************************
#pragma mark - private
