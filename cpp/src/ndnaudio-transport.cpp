//
//  ndnaudio-transport.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 10/21/13
//

#include "ndnaudio-transport.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
//******************************************************************************



//******************************************************************************
//******************************************************************************


//******************************************************************************
#pragma mark - construction/destruction

NdnAudioTransport::NdnAudioTransport()
{
    
}
NdnAudioTransport::~NdnAudioTransport()
{
    
}

//******************************************************************************
#pragma mark - public
int NdnAudioTransport::init(VoENetwork *voiceEngine)
{
    voiceEngine_ = voiceEngine;
    
    return 0;
}

int NdnAudioTransport::start()
{
    started_ = true;
    
    return 0;
}

int NdnAudioTransport::stop()
{
    started_ = false;
    
    return 0;
}

//******************************************************************************
#pragma mark - intefaces realization - Transport
int NdnAudioTransport::SendPacket(int channel, const void *data, int len)
{
    TRACE("sending RTP packet of length %d", len);
    
    // return bytes sent or negative value on error
    return len;
}

int NdnAudioTransport::SendRTCPPacket(int channel, const void *data, int len)
{
    TRACE("sending RTCP packet of length %d", len);
    
    // return bytes sent or negative value on error
    return len;
}

//******************************************************************************
#pragma mark - private
