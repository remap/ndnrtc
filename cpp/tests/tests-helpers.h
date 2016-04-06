// 
// tests-helpers.h
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __tests_helpers_h__
#define __tests_helpers_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <include/params.h>
#include <include/interfaces.h>
#include <include/statistics.h>

#include "src/slot-buffer.h"
#include "client/src/config.h"

ndnrtc::new_api::VideoCoderParams sampleVideoCoderParams();
ClientParams sampleConsumerParams();
ClientParams sampleProducerParams();

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

class MockExternalCapturer : public ndnrtc::IExternalCapturer
{
public:
	MOCK_METHOD0(capturingStarted, void(void));
	MOCK_METHOD0(capturingStopped, void(void));
	MOCK_METHOD4(incomingArgbFrame, int(const unsigned int width,
                                      const unsigned int height,
                                      unsigned char* argbFrameData,
                                      unsigned int frameSize));
	MOCK_METHOD8(incomingI420Frame, int(const unsigned int width,
                                      const unsigned int height,
                                      const unsigned int strideY,
                                      const unsigned int strideU,
                                      const unsigned int strideV,
                                      const unsigned char* yBuffer,
                                      const unsigned char* uBuffer,
                                      const unsigned char* vBuffer));
};

class MockNdnKeyChain 
{
public:
	MOCK_METHOD3(verifyData, void(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed));

	void callOnVerifySuccess(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed)

	{
		onVerified(data);
	}

	void callOnVerifyFailed(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed)
	{
		onVerifyFailed(data);
	}
};

class MockSlotBuffer : public ndnrtc::ISlotBuffer {
public:
	MOCK_METHOD3(accessSlotExclusively, void(const std::string&,
		const ndnrtc::OnSlotAccess&, const ndnrtc::OnNotFound&));

	void callOnSlotAccess(const std::string&,
		const ndnrtc::OnSlotAccess& onSlotAccess, const ndnrtc::OnNotFound&)
	{
		onSlotAccess(slot_);
	}

	void callOnSlotNotFound(const std::string&,
		const ndnrtc::OnSlotAccess&, const ndnrtc::OnNotFound& onNotFound)
	{
		onNotFound();
	}

	boost::shared_ptr<ndnrtc::ISlot> slot_;
};

class MockSlot : public ndnrtc::ISlot {
public:
	MOCK_METHOD2(markVerified, void(const std::string&, bool));
};

namespace testing
{
 namespace internal
 {
  enum GTestColor {
      COLOR_DEFAULT,
      COLOR_RED,
      COLOR_GREEN,
      COLOR_YELLOW
  };

  extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
 }
}
#define GT_PRINTF(...)  do { testing::internal::ColoredPrintf(testing::internal::COLOR_GREEN, "[ INFO     ] "); testing::internal::ColoredPrintf(testing::internal::COLOR_YELLOW, __VA_ARGS__); } while(0)

#endif
