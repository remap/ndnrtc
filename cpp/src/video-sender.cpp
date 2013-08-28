//
//  video-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
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
    TRACE("register conference");
}

void NdnVideoSender::fetchParticipantsList()
{
    TRACE("fetch participants");
}

//********************************************************************************
#pragma mark - private
