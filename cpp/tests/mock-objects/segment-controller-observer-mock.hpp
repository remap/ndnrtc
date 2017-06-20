// 
// segment-controller-observer-mock.hpp
//
//  Created by Peter Gusev on 04 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_observer_mock_h__
#define __buffer_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/segment-controller.hpp"

class MockSegmentControllerObserver : public ndnrtc::ISegmentControllerObserver
{
public:
	MOCK_METHOD1(segmentArrived, void(const boost::shared_ptr<ndnrtc::WireSegment>&));
	MOCK_METHOD1(segmentRequestTimeout, void(const ndnrtc::NamespaceInfo&));
	MOCK_METHOD2(segmentNack, void(const ndnrtc::NamespaceInfo&, int));
	MOCK_METHOD0(segmentStarvation, void());
};

#endif
