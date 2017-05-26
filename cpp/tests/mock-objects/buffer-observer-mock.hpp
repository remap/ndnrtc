// 
// buffer-observer-mock.hpp
//
//  Created by Peter Gusev on 02 May 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_observer_mock_h__
#define __buffer_observer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/frame-buffer.hpp"

class MockBufferObserver : public ndnrtc::IBufferObserver
{
public:
	MOCK_METHOD1(onNewRequest, void(const boost::shared_ptr<ndnrtc::BufferSlot>&));
	MOCK_METHOD1(onNewData, void(const ndnrtc::BufferReceipt&));
    MOCK_METHOD0(onReset, void(void));
};

#endif
