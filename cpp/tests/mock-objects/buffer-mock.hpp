// 
// buffer-mock.hpp
//
//  Created by Peter Gusev on 15 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __buffer_mock_h__
#define __buffer_mock_h__

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "frame-buffer.hpp"

class MockBuffer : public ndnrtc::IBuffer {
public:
	MOCK_METHOD0(reset, void());
	MOCK_METHOD1(requested, bool(const std::vector<boost::shared_ptr<const ndn::Interest>>&));
	MOCK_METHOD1(received, ndnrtc::BufferReceipt(const boost::shared_ptr<ndnrtc::WireSegment>&));
	MOCK_CONST_METHOD1(isRequested, bool(const boost::shared_ptr<ndnrtc::WireSegment>&));
	MOCK_CONST_METHOD2(getSlotsNum, unsigned int(const ndn::Name&, int));
    MOCK_CONST_METHOD0(shortdump, std::string());
    MOCK_METHOD1(attach, void(ndnrtc::IBufferObserver*));
    MOCK_METHOD1(detach, void(ndnrtc::IBufferObserver*));
};

#endif