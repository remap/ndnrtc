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
#include "client/src/config.h"
#include <include/interfaces.h>
#include <include/statistics.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include "src/frame-data.h"

ndnrtc::VideoCoderParams sampleVideoCoderParams();
ClientParams sampleConsumerParams();
ClientParams sampleProducerParams();

webrtc::EncodedImage encodedImage(size_t frameLen, uint8_t*& buffer, bool delta = true);
bool checkVideoFrame(const webrtc::EncodedImage& image);
ndnrtc::VideoFramePacket getVideoFramePacket(size_t frameLen = 4300, double rate = 24.7,
  int64_t pubTs = 488589553, int64_t pubUts = 1460488589);
std::vector<ndnrtc::VideoFrameSegment> sliceFrame(ndnrtc::VideoFramePacket& vp, 
  PacketNumber playNo = 0, PacketNumber pairedSeqNo = 1);
std::vector<ndnrtc::CommonSegment> sliceParity(ndnrtc::VideoFramePacket& vp);
std::vector<boost::shared_ptr<ndn::Data>> dataFromSegments(std::string frameName,
	const std::vector<ndnrtc::VideoFrameSegment>& segments);
std::vector<boost::shared_ptr<ndn::Data>> dataFromParitySegments(std::string frameName,
	const std::vector<ndnrtc::CommonSegment>& segments);
std::vector<boost::shared_ptr<ndn::Interest>> getInterests(std::string frameName,
	unsigned int startSeg, size_t nSeg);

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
