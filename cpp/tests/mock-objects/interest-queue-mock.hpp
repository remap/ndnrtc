// 
// interest-queue-mock.hpp
//
//  Created by Peter Gusev on 13 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __interest_queue_mock_h__
#define __interest_queue_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "interest-queue.hpp"

class MockInterestQueue : public ndnrtc::IInterestQueue {
public:
	MOCK_METHOD5(enqueueInterest, void(const boost::shared_ptr<const ndn::Interest>&,
                        boost::shared_ptr<ndnrtc::DeadlinePriority>, ndnrtc::OnData, 
                        ndnrtc::OnTimeout, ndnrtc::OnNetworkNack));
	MOCK_METHOD0(reset, void(void));
};

#endif
