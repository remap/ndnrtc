// 
// ndnrtc-library-mock.h
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __ndnrtc_library_mock_h__
#define __ndnrtc_library_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <include/interfaces.h>

class MockNdnRtcLibrary : public ndnrtc::INdnRtcLibrary
{
public:
	MOCK_METHOD1(setObserver, void(ndnrtc::INdnRtcLibraryObserver*));
	MOCK_METHOD5(startSession, std::string(const std::string&, const std::string&, 
		const ndnrtc::new_api::GeneralParams&, ndn::KeyChain*, ndnrtc::ISessionObserver*));
	MOCK_METHOD1(stopSession, int(const std::string&));
	MOCK_METHOD3(addLocalStream, std::string(const std::string&, const ndnrtc::new_api::MediaStreamParams&, 
		ndnrtc::IExternalCapturer** const));
	MOCK_METHOD2(removeLocalStream, int(const std::string&, const std::string&));
	MOCK_METHOD7(addRemoteStream, std::string(const std::string&, const std::string&,
		const ndnrtc::new_api::MediaStreamParams&, const ndnrtc::new_api::GeneralParams&, 
		const ndnrtc::new_api::GeneralConsumerParams&, ndn::KeyChain* keyChain,
		ndnrtc::IExternalRenderer* const));
	MOCK_METHOD1(removeRemoteStream, std::string(const std::string&));
	MOCK_METHOD2(setStreamObserver, int(const std::string&, ndnrtc::IConsumerObserver* const));
	MOCK_METHOD1(removeStreamObserver, int(const std::string&));
	MOCK_METHOD1(getStreamThread, std::string(const std::string&));
	MOCK_METHOD2(switchThread, int(const std::string&, const std::string&));
	MOCK_METHOD1(getRemoteStreamStatistics, ndnrtc::new_api::statistics::StatisticsStorage(const std::string&));
	MOCK_METHOD1(getVersionString, void(char**));
	MOCK_METHOD3(serializeSessionInfo, void(const ndnrtc::new_api::SessionInfo&,
		unsigned int&, unsigned char **));
	MOCK_METHOD3(deserializeSessionInfo, bool(const unsigned int, 
		const unsigned char *, ndnrtc::new_api::SessionInfo&));
};

#endif