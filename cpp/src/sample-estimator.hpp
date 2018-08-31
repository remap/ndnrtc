
// 
// sample-estimator.hpp
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __sample_estimator_h__
#define __sample_estimator_h__

#include "segment-controller.hpp"
#include "estimators.hpp"
#include "name-components.hpp"

namespace ndn {
    class Interest;
}

namespace ndnrtc {
    namespace statistics {
        class StatisticsStorage;
    }
    
    /**
     * This class runs average estimation of sample size and number of segments
     * per sample. It supports two sample classes - Delta and Key and two segment
     * data classes  - Data and Parity.
     */
	class SampleEstimator : public ISegmentControllerObserver {
	public:
        SampleEstimator(const boost::shared_ptr<statistics::StatisticsStorage>&);
		~SampleEstimator();
        
        /**
         * This initializes average estimator of the number of segments per sample
         */
        void bootstrapSegmentNumber(double value, SampleClass st, SegmentClass dt);

        /**
         * This initializes average estimator of the segment size per sample
         */
        void bootstrapSegmentSize(double value, SampleClass st, SegmentClass dt);
        
        /**
         * Called by SegmentController each time new segment arrives
         */
		void segmentArrived(const boost::shared_ptr<WireSegment>&);
        
        /**
         * Resets estimatior
         */
		void reset();
        
        /**
         * Returns estimation of segments number per sample and segment class
         * @param st Sample class - Key or Delta
         * @param dt Segment class - Data or Parity
         * @return Average estimation of the number of segments for sample/segment 
         *          class requested
         */
		double getSegmentNumberEstimation(SampleClass st, SegmentClass dt);
        
        /**
         * Returns estimation of segment size per sample and segment class
         * @param st Sample class - Key or Delta
         * @param dt Segment class - Data or Parity
         * @return Average estimation of the nuber of bytes per one segment for 
         *          sample/segment class requested
         */
		double getSegmentSizeEstimation(SampleClass st, SegmentClass dt);

	private:
		typedef struct _Estimators {
			_Estimators();
			~_Estimators();

			estimators::Average segNum_, segSize_;
		} Estimators;
		typedef std::map<std::pair<SampleClass, SegmentClass>, Estimators> EstimatorMap;
		EstimatorMap estimators_;
        boost::shared_ptr<statistics::StatisticsStorage> sstorage_;

		void segmentRequestTimeout(const NamespaceInfo&, 
                                   const boost::shared_ptr<const ndn::Interest> &){}
        void segmentNack(const NamespaceInfo&, int, 
                         const boost::shared_ptr<const ndn::Interest> &){}
		void segmentStarvation(){}
	};
}

#endif
