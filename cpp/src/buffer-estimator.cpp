//
//  buffer-estimator.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "buffer-estimator.h"
#include "rtt-estimation.h"

using namespace ndnrtc::new_api;

// minimal buffer size in milliseconds
const int64_t BufferEstimator::MinBufferSizeMs = 100;

//******************************************************************************
#pragma mark - public
void
BufferEstimator::setProducerRate(double producerRate)
{
}

int64_t
BufferEstimator::getTargetSize()
{
    double rttEstimate = RttEstimation::sharedInstance().getCurrentEstimation();
    
    if (rttEstimate > MinBufferSizeMs)
        return rttEstimate;
    
    return minBufferSizeMs_;
}