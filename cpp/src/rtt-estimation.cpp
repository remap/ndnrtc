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

using namespace std;
using namespace ndnlog;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;

const double RttEstimation::RttStartEstimate = 30; // millseconds

//******************************************************************************
#pragma mark - construction/destruction
RttEstimation::RttEstimation(const boost::shared_ptr<statistics::StatisticsStorage>& statStorage,
                             const double& startEstimate):
StatObject(statStorage),
estimatorId_(NdnRtcUtils::setupMeanEstimator(0, startEstimate))
{}

RttEstimation::~RttEstimation()
{}

//******************************************************************************
#pragma mark - public
double
RttEstimation::updateEstimation(int64_t rountripTimeMs,
                                int64_t generationDelay)
{
    double rawValue = rountripTimeMs-generationDelay;
    
    if (rawValue > 0)
    {
        NdnRtcUtils::meanEstimatorNewValue(estimatorId_, rawValue);
        
        double current = getCurrentEstimation();
        (*statStorage_)[Indicator::RttEstimation] = current;
        
        LogTraceC
        << "updated estimation. round " << rountripTimeMs <<
        " generation " << generationDelay
        << " raw " << rawValue
        << " est " << current << std::endl;
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

void
RttEstimation::reset()
{
    LogTraceC << "reset" << endl;
    NdnRtcUtils::resetMeanEstimator(estimatorId_);
}
