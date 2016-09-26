// 
// interest-queue-observer-mock.hpp
//
//  Created by Peter Gusev on 11 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __interest_queue_observer_mock_h__
#define __interest_queue_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "interest-queue.hpp"

class MockInterestQueueObserver : public ndnrtc::IInterestQueueObserver
{
public:
	MOCK_METHOD1(onInterestIssued, void(const boost::shared_ptr<const ndn::Interest>&));
};

#endif