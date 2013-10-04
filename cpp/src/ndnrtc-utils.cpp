//
//  ndnrtc-utils.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#undef NDN_LOGGING

#include "ndnrtc-utils.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - all static

unsigned int NdnRtcUtils::getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize)
{
    return (unsigned int)ceil((float)dataSize/(float)segmentSize);
}

int NdnRtcUtils::segmentNumber(const Name::Component &segmentComponent)
{
    std::vector<unsigned char> bytes = *segmentComponent.getValue();
    int bytesLength = segmentComponent.getValue().size();
    double result = 0.0;
    unsigned int i;
    
    for (i = 0; i < bytesLength; ++i) {
        result *= 256.0;
        result += (double)bytes[i];
    }
    return (unsigned int)result;
}

int NdnRtcUtils::frameNumber(const Name::Component &segmentComponent)
{
    unsigned int result = 0;
    int valueLength = segmentComponent.getValue().size();
    std::vector<unsigned char> value = *segmentComponent.getValue();
    
    unsigned int i;
    for (i = 0; i < valueLength; ++i) {
        unsigned char digit = value[i];
        if (!(digit >= '0' && digit <= '9'))
            return -1;
        
        result *= 10;
        result += (unsigned int)(digit - '0');
    }
    
    return result;
}

int64_t NdnRtcUtils::millisecondTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    int64_t ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
    
    return ticks;
};

int64_t NdnRtcUtils::microsecondTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    int64_t ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec);
    
    return ticks;
};