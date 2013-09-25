//
//  ndnrtc-utils.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "ndnrtc-utils.h"

using namespace ndnrtc;

//********************************************************************************
#pragma mark - all static

unsigned int NdnRtcUtils::getSegmentsNumber(unsigned int segmentSize, unsigned int dataSize)
{
    return (unsigned int)ceil((float)dataSize/(float)segmentSize);
}

unsigned int NdnRtcUtils::segmentNumber(const Name::Component &segmentComponent)
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