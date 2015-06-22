//
//  chase-estimator.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__chase_estimator__
#define __ndnrtc__chase_estimator__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace new_api {
        class ChaseEstimation : public ndnlog::new_api::ILoggingObject
        {
        public:
            
            static const unsigned int SampleSize;
            static const double ChangeThreshold;
            static const double FilterCoeff;
            static const int InclineEstimatorSample;
            static const int MinOccurences;
            
            ChaseEstimation(unsigned int sampleSize = SampleSize,
                            double changeThreshold = ChangeThreshold,
                            double filterCoeff = FilterCoeff);
            ~ChaseEstimation();
            
            void
            trackArrival(double currentRate = 0.);
            
            double
            getArrivalEstimation();
            
            bool
            isArrivalStable();
            
            void
            reset(bool resetStartTime = false);
          
        private:
            unsigned int sampleSize_;
            double changeThreshold_;
            unsigned int arrivalDelayEstimatorId_,
                        inclineEstimator_;
            int64_t lastArrivalTimeMs_, startTime_;
            double lastPacketRateUpdate_ = 33.;

            int nStabilizedOccurences_;
            bool stabilized_;
            double stabilizedValue_, lastCheckedValue_;
        };
        
        class BaseStabilityEstimator : public ndnlog::new_api::ILoggingObject
        {
        public:
            BaseStabilityEstimator(unsigned int sampleSize,
                                   unsigned int minStableOccurrences,
                                   double threshold);
            virtual ~BaseStabilityEstimator(){}
            
            double
            getMeanValue();
            
            bool
            isStable()
            { return isStable_; }

        protected:
            unsigned int sampleSize_, minOccurrences_;
            double threshold_;
            unsigned int meanEstimatorId_;
            unsigned int nStableOccurrences_, nUnstableOccurrences_;
            bool isStable_;
        };
        
        class StabilityEstimator : public BaseStabilityEstimator
        {
        public:
            StabilityEstimator(unsigned int sampleSize,
                               unsigned int minStableOccurrences,
                               double threshold,
                               double rateSimilarityLevel);
            ~StabilityEstimator(){}
            
            void
            trackInterArrival(double currentRate);
            
            unsigned int
            getLastDelta()
            { return lastDelta_; }
            
            void
            flush();
            
        private:
            double rateSimilarityLevel_;
            uint64_t lastTimestamp_;
            unsigned int lastDelta_;
        };
        
        class RttChangeEstimator : public BaseStabilityEstimator
        {
        public:
            RttChangeEstimator(unsigned int sampleSize,
                               unsigned int minStableOccurrences,
                               double threshold);
            ~RttChangeEstimator(){}
            
            void
            newRttValue(double rtt);
            
            bool
            hasChange();
            
            void
            flush();
            
        private:
            unsigned int nChanges_;
            unsigned int nMinorConsecutiveChanges_;
            unsigned int lastCheckedChangeNumber_;
        };
    }
}

#endif /* defined(__ndnrtc__chase_estimator__) */
