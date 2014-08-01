//
//  chase-estimator.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "chase-estimation.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc::new_api;

const unsigned int ChaseEstimation::SampleSize = 0;
const double ChaseEstimation::ChangeThreshold = .7;
const double ChaseEstimation::FilterCoeff = 0.07;
const int ChaseEstimation::InclineEstimatorSample = 3;
const int ChaseEstimation::MinOccurences = 1;

#define RATE_SIMILARITY_LVL 0.7

//******************************************************************************
#pragma mark - construction/destruction
ChaseEstimation::ChaseEstimation(unsigned int sampleSize,
                                 double changeThreshold,
                                 double filterCoeff):
sampleSize_(sampleSize),
changeThreshold_(changeThreshold),
arrivalDelayEstimatorId_(NdnRtcUtils::setupMeanEstimator(sampleSize)),
inclineEstimator_(NdnRtcUtils::setupInclineEstimator(InclineEstimatorSample)),
lastArrivalTimeMs_(-1),
nStabilizedOccurences_(0),
stabilized_(false),
stabilizedValue_(0.),
lastCheckedValue_(0),
startTime_(0)
{
}

ChaseEstimation::~ChaseEstimation()
{
    NdnRtcUtils::releaseMeanEstimator(arrivalDelayEstimatorId_);
    NdnRtcUtils::releaseInclineEstimator(inclineEstimator_);
}

//******************************************************************************
#pragma mark - public
void
ChaseEstimation::trackArrival(double currentRate)
{
    int64_t arrivalTimestamp = NdnRtcUtils::millisecondTimestamp();

    if (currentRate > 0)
        lastPacketRateUpdate_ = currentRate;
    
    if (startTime_ == 0)
        startTime_ = arrivalTimestamp;
    
    if (lastArrivalTimeMs_ != -1)
    {
        double delay = (double)(arrivalTimestamp-lastArrivalTimeMs_);
        
        NdnRtcUtils::meanEstimatorNewValue(arrivalDelayEstimatorId_, delay);

        NdnRtcUtils::inclineEstimatorNewValue(inclineEstimator_, NdnRtcUtils::currentMeanEstimation(arrivalDelayEstimatorId_));
        
        LogStatC
        << "\tdelay\t" << delay << "\tmean\t"
        << NdnRtcUtils::currentMeanEstimation(arrivalDelayEstimatorId_)
        << "\trate\t" << lastPacketRateUpdate_
        << "\tincline\t" << NdnRtcUtils::currentIncline(inclineEstimator_)
        << endl;
        
        if (!stabilized_)
        {
            double incline = fabs(NdnRtcUtils::currentIncline(inclineEstimator_));
            double mean = NdnRtcUtils::currentMeanEstimation(arrivalDelayEstimatorId_);
            
            if (incline != 0 &&
                incline < changeThreshold_ &&
                mean >= RATE_SIMILARITY_LVL*(1000./lastPacketRateUpdate_))
            {
                lastCheckedValue_ = incline;
                nStabilizedOccurences_++;
                LogStatC << "\tstable sequential occurence #\t" << nStabilizedOccurences_ << endl;
            }
            else
                nStabilizedOccurences_ = 0;
            
            if (nStabilizedOccurences_ > MinOccurences*InclineEstimatorSample)
            {
                stabilized_ = true;
                stabilizedValue_ = incline;
                
                LogStatC
                << "\tfinished in\t" << NdnRtcUtils::millisecondTimestamp() - startTime_
                << "" << endl;
            }
        }
    }
    
    lastArrivalTimeMs_ = arrivalTimestamp;
}

double
ChaseEstimation::getArrivalEstimation()
{
    if (stabilized_)
        return stabilizedValue_;
    
    return NdnRtcUtils::currentMeanEstimation(arrivalDelayEstimatorId_);
}

bool
ChaseEstimation::isArrivalStable()
{
    return stabilized_;
}

void
ChaseEstimation::reset(bool resetStartTime)
{
    LogStatC << "\treset" << endl;
    
    if (resetStartTime)
        startTime_ = 0;
    
    lastArrivalTimeMs_ = -1;
    nStabilizedOccurences_ = 0;
    stabilized_ = false;
    stabilizedValue_ = 0.;
    lastCheckedValue_ = 0;
    
    NdnRtcUtils::releaseMeanEstimator(arrivalDelayEstimatorId_);
    NdnRtcUtils::releaseInclineEstimator(inclineEstimator_);
    
    arrivalDelayEstimatorId_ = NdnRtcUtils::setupMeanEstimator(sampleSize_);
    inclineEstimator_ = NdnRtcUtils::setupInclineEstimator(InclineEstimatorSample);
}