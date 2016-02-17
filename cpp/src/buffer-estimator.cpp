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

using namespace boost;
using namespace ndnrtc::new_api;

// minimal buffer size in milliseconds
const int64_t BufferEstimator::MinBufferSizeMs = 250;

//******************************************************************************
#pragma mark - construction/destruction
BufferEstimator::BufferEstimator(double alpha, double beta,
                                 const shared_ptr<RttEstimation>& rttEstimation,
                                 int64_t minBufferSizeMs):
alpha_(alpha),
beta_(beta),
rttEstimation_(rttEstimation),
minBufferSizeMs_(minBufferSizeMs)
{
}

//******************************************************************************
#pragma mark - public
void
BufferEstimator::setProducerRate(double producerRate)
{
}

int64_t
BufferEstimator::getTargetSize()
{
    double rttEstimate = rttEstimation_->getCurrentEstimation();
    double variation = rttEstimation_->getCurrentVariation();

#if 0
    // we set buffer target size to be 2*RTT or
    // minimal buffer size specified by user (or default)
    
    if (rttEstimate*2 > minBufferSizeMs_)
        return rttEstimate*2;
    
    return minBufferSizeMs_;
#else
    return alpha_*variation + beta_*rttEstimate;
#endif
}