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
	MOCK_METHOD1(segmentArrived, void(const std::shared_ptr<ndnrtc::WireSegment>&));
	MOCK_METHOD2(segmentRequestTimeout, void(const ndnrtc::NamespaceInfo&, const std::shared_ptr<const ndn::Interest> &));
	MOCK_METHOD3(segmentNack, void(const ndnrtc::NamespaceInfo&, int, const std::shared_ptr<const ndn::Interest> &));
	MOCK_METHOD0(segmentStarvation, void());
};

#endif
