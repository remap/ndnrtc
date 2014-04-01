//
//  rtt-estimator.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "rtt-estimation.h"
#include "ndnrtc-utils.h"

using namespace ndnlog;
using namespace ndnrtc::new_api;

RttEstimation RttEstimation::sharedRttEstimation_ = RttEstimation();
const double RttEstimation::RttStartEstimate = 30; // millseconds

//******************************************************************************
#pragma mark - construction/destruction
RttEstimation::RttEstimation(const double& startEstimate):
estimatorId_(NdnRtcUtils::setupMeanEstimator(0, startEstimate))
{}

RttEstimation::~RttEstimation()
{}

//******************************************************************************
#pragma mark - public
RttEstimation&
RttEstimation::sharedInstance()
{
    return sharedRttEstimation_;
}

double
RttEstimation::updateEstimation(int64_t rountripTimeMs,
                                int64_t generationDelay)
{
    double rawValue = rountripTimeMs-generationDelay;
    
    if (rawValue > 0)
    {
        NdnRtcUtils::meanEstimatorNewValue(estimatorId_, rawValue);
        
        LogTraceC << "rtt estimate\t" <<
        NdnRtcUtils::currentMeanEstimation(estimatorId_) << endl;
    }
    else
    {
        LogWarnC
        << "wrong data for RTT "
        << rountripTimeMs << " " << generationDelay << endl;
    }
    
    return NdnRtcUtils::currentMeanEstimation(estimatorId_);
}

double
RttEstimation::getCurrentEstimation() const
{
    return NdnRtcUtils::currentMeanEstimation(estimatorId_);
}