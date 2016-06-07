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

namespace ndnrtc {
	class SampleEstimator : public ISegmentControllerObserver {
	public:
		typedef enum _SampleType {
			Delta,
			Key
		} SampleType;
		typedef enum _DataType {
			Parity,
			Data
		} DataType;

		SampleEstimator();
		~SampleEstimator();

		void segmentArrived(const boost::shared_ptr<WireSegment>&);
		void reset();
		double getSegmentNumberEstimation(SampleType st, DataType dt);
		double getSegmentSizeEstimation(SampleType st, DataType dt);

	private:
		typedef struct _Estimators {
			_Estimators();
			~_Estimators();

			estimators::Average segNum_, segSize_;
		} Estimators;

		std::map<std::pair<SampleType, DataType>, Estimators> estimators_;

		void segmentRequestTimeout(const NamespaceInfo&){}
		void segmentStarvation(){}
	};
}

#endif