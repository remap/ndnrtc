// 
// stream-mock.hpp
//
//  Created by Peter Gusev on 19 July 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __stream_mock_h__
#define __stream_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ndnrtc/stream.hpp>

class MockStream : public ndnrtc::IStream {
public:
	MOCK_CONST_METHOD0(getBasePrefix, std::string());
	MOCK_CONST_METHOD0(getStreamName, std::string());
	MOCK_CONST_METHOD0(getPrefix, std::string());
	MOCK_CONST_METHOD0(getStatistics, ndnrtc::statistics::StatisticsStorage());
	MOCK_METHOD1(setLogger, void(boost::shared_ptr<ndnlog::new_api::Logger>));
};

#endif