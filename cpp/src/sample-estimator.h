// 
// sample-estimator.h
//
//  Created by Peter Gusev on 06 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __sample_estimator_h__
#define __sample_estimator_h__

#include "segment-controller.h"
#include "estimators.h"
#include "name-components.h"

namespace ndnrtc {
	class SampleEstimator : public ISegmentControllerObserver {
	public:
		SampleEstimator();
		~SampleEstimator();

		void segmentArrived(const boost::shared_ptr<WireSegment>&);
		void reset();
		double getSegmentNumberEstimation(SampleClass st, SegmentClass dt);
		double getSegmentSizeEstimation(SampleClass st, SegmentClass dt);

	private:
		typedef struct _Estimators {
			_Estimators();
			~_Estimators();

			estimators::Average segNum_, segSize_;
		} Estimators;

		std::map<std::pair<SampleClass, SegmentClass>, Estimators> estimators_;

		void segmentRequestTimeout(const NamespaceInfo&){}
		void segmentStarvation(){}
	};
}

#endif