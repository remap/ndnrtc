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

//******************************************************************************
//******************************************************************************
BaseStabilityEstimator::BaseStabilityEstimator(unsigned int sampleSize,
                                               unsigned int minStableOccurrences,
                                               double threshold):
sampleSize_(sampleSize),
minOccurrences_(minStableOccurrences),
threshold_(threshold),
meanEstimatorId_(NdnRtcUtils::setupSlidingAverageEstimator(sampleSize_)),
nStableOccurrences_(0),
nUnstableOccurrences_(0),
isStable_(false)
{
}

double
BaseStabilityEstimator::getMeanValue()
{
    return NdnRtcUtils::currentSlidingAverageValue(meanEstimatorId_);
}

//******************************************************************************
StabilityEstimator::StabilityEstimator(unsigned int sampleSize,
                                       unsigned int minStableOccurrences,
                                       double threshold,
                                       double rateSimilarityLevel):
BaseStabilityEstimator(sampleSize, minStableOccurrences, threshold),
rateSimilarityLevel_(rateSimilarityLevel),
lastDelta_(0),
lastTimestamp_(0)
{
    description_ = "stability-estimator";
}

void
StabilityEstimator::trackInterArrival(double currentRate)
{
    if (lastTimestamp_ != 0)
    {
        unsigned int delta = NdnRtcUtils::millisecondTimestamp() - lastTimestamp_;
        lastDelta_ = delta;
        
        NdnRtcUtils::slidingAverageEstimatorNewValue(meanEstimatorId_, (double)delta);
        
        double mean = NdnRtcUtils::currentSlidingAverageValue(meanEstimatorId_);
        
        if (mean != 0)
        {
            double deviationPercentage = NdnRtcUtils::currentSlidingDeviationValue(meanEstimatorId_)/mean;
            double targetDelay = 1000./currentRate;

            if (deviationPercentage <= threshold_)
            {
                nUnstableOccurrences_ = 0;
                nStableOccurrences_++;

                if (!isStable_)
                    LogTraceC << "stable occurrence #" << nStableOccurrences_ << std::endl;
            }
            else if (++nUnstableOccurrences_ >= minOccurrences_)
                nStableOccurrences_ = 0;
            
            isStable_ = (nStableOccurrences_ >= minOccurrences_);
            
            LogTraceC
            << "delta\t" << delta
            << "\tmean\t" << mean
            << "\tdeviation\t" << NdnRtcUtils::currentSlidingDeviationValue(meanEstimatorId_)
            << "\tdeviation %\t" << deviationPercentage
            << "\trate\t" << currentRate
            << "\ttarget delay\t" << targetDelay
            << "\tsim level\t" << fabs(mean-targetDelay)/targetDelay
            << "\tstable\t" << (isStable_?"YES":"NO")
            << std::endl;
        }
        else
            LogTraceC
            << "delta\t" << delta
            << "\tmean\t" << mean
            << "\tstable\t" << (isStable_?"YES":"NO")
            << std::endl;
    }
    
    lastTimestamp_ = NdnRtcUtils::millisecondTimestamp();
}

void
StabilityEstimator::flush()
{
    isStable_ = false;
    nStableOccurrences_ = 0;
    nUnstableOccurrences_ = 0;
    lastTimestamp_ = 0;
    lastDelta_ = 0;
}

//******************************************************************************
RttChangeEstimator::RttChangeEstimator(unsigned int sampleSize,
                   unsigned int minStableOccurrences,
                   double threshold):
BaseStabilityEstimator(sampleSize, minStableOccurrences, threshold)
{
    description_ = "rtt-change-estimator";
}

void
RttChangeEstimator::newRttValue(double rtt)
{
    NdnRtcUtils::slidingAverageEstimatorNewValue(meanEstimatorId_, rtt);
    double mean = NdnRtcUtils::currentSlidingAverageValue(meanEstimatorId_);
    
    if (mean != 0)
    {
        double deviationPercentage = NdnRtcUtils::currentSlidingDeviationValue(meanEstimatorId_)/mean;
        
        if (deviationPercentage <= threshold_)
            nStableOccurrences_++;
        else
        {
            isStable_ = false;
            nChanges_ = 0;
            nMinorConsecutiveChanges_ = 0;
            nStableOccurrences_ = 0;
        }
        
        isStable_ = (nStableOccurrences_ >= minOccurrences_);
        
        if (isStable_)
        {
            double changePercentage = fabs(rtt-mean)/mean;
            if (changePercentage >= 0.08)
            {
                if (changePercentage <= 0.2)
                {
                    nMinorConsecutiveChanges_++;
                    if (nMinorConsecutiveChanges_ >= sampleSize_)
                    {
                        nMinorConsecutiveChanges_ = 0;
                        nChanges_++;
                    }
                }
                else
                    nChanges_++;
            }
            else
                nMinorConsecutiveChanges_ = 0;
        }
        
        LogTraceC
        << "rtt\t" << rtt
        << "\tmean\t" << mean
        << "\tdeviation\t" << deviationPercentage
        << "\tn changes\t" << nChanges_
        << "\tn minor\t" << nMinorConsecutiveChanges_
        << "\tstable\t" << (isStable_?"YES":"NO")
        << std::endl;
    }
}

bool
RttChangeEstimator::hasChange()
{
    bool result = false;
    
    if (nChanges_ > lastCheckedChangeNumber_)
        result = true;
    
    lastCheckedChangeNumber_ = nChanges_;
    return result;
}

void
RttChangeEstimator::flush()
{
    isStable_ = false;
    nStableOccurrences_ = 0;
    nChanges_ = 0;
    nMinorConsecutiveChanges_ = 0;
    lastCheckedChangeNumber_ = 0;
}
