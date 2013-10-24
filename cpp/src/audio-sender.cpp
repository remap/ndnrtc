//
//  audio-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "audio-sender.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
//******************************************************************************
#pragma mark - static
int NdnAudioSender::getStreamControlPrefix(const ParamsStruct &params,
                                           string &prefix)
{
    int res = RESULT_OK;
    string streamPrefix;
    
    MediaSender::getStreamPrefix(params, streamPrefix);
    
    string streamThread = ParamsStruct::validate(params.streamThread,
                                                 DefaultParams.streamThread,
                                                 res);
    string rtcpSuffix = "control";
    
    prefix = *NdnRtcNamespace::buildPath(false,
                                        &streamPrefix,
                                        &streamThread,
                                        &rtcpSuffix,
                                        NULL);
    
    return res;
}

//******************************************************************************
#pragma mark - public
int NdnAudioSender::init(const shared_ptr<ndn::Transport> transport)
{
    int res = RESULT_OK;
    
    res = MediaSender::init(transport);
    
    if (RESULT_FAIL(res))
        return res;

        string prefix;

    res = NdnAudioSender::getStreamControlPrefix(params_, prefix);

    if (RESULT_GOOD(res))
        rtcpPacketPrefix_.reset(new Name(prefix.c_str()));
    
    return res;
}

int NdnAudioSender::publishRTPAudioPacket(unsigned int len, unsigned char *data)
{
    publishPacket(len, data);
    packetNo_++;
    
    return 0;
}

int NdnAudioSender::publishRTCPAudioPacket(unsigned int len, unsigned char *data)
{
    publishPacket(len, data, rtcpPacketPrefix_, rtcpPacketNo_);
    rtcpPacketNo_++;
    
    return 0;
}
//******************************************************************************
#pragma mark - intefaces realization - <#interface#>

//******************************************************************************
#pragma mark - private

