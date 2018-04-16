//
// tests-helpers.hpp
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __tests_helpers_h__
#define __tests_helpers_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>
#include <boost/thread/recursive_mutex.hpp>

#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>

#include <include/params.hpp>
#include "client/src/config.hpp"
#include <include/interfaces.hpp>
#include <include/statistics.hpp>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>

#include "src/frame-data.hpp"
#include "name-components.hpp"

#define GT_PRINTF(...)                                                                     \
    do                                                                                     \
    {                                                                                      \
        testing::internal::ColoredPrintf(testing::internal::COLOR_GREEN, "[ INFO     ] "); \
        testing::internal::ColoredPrintf(testing::internal::COLOR_YELLOW, __VA_ARGS__);    \
    } while (0)

ndnrtc::VideoCoderParams
sampleVideoCoderParams();
ClientParams
sampleConsumerParams();
ClientParams
sampleProducerParams();

WebRtcVideoFrame getFrame(int w, int h, bool randomNoise = false);
std::vector<WebRtcVideoFrame> getFrameSequence(int w, int h, int len);

webrtc::EncodedImage
encodedImage(size_t frameLen, uint8_t *&buffer, bool delta = true);

bool checkVideoFrame(const webrtc::EncodedImage &image);

ndnrtc::VideoFramePacket
getVideoFramePacket(size_t frameLen = 4300, double rate = 24.7, int64_t pubTs = 488589553,
                    int64_t pubUts = 1460488589);

std::vector<ndnrtc::VideoFrameSegment>
sliceFrame(ndnrtc::VideoFramePacket &vp, PacketNumber playNo = 0, PacketNumber pairedSeqNo = 1);

std::vector<ndnrtc::VideoFrameSegment>
sliceParity(ndnrtc::VideoFramePacket &vp, boost::shared_ptr<ndnrtc::NetworkData> &parity);

std::vector<boost::shared_ptr<ndn::Data>>
dataFromSegments(std::string frameName, const std::vector<ndnrtc::VideoFrameSegment> &segments);

std::vector<boost::shared_ptr<ndn::Data>>
dataFromParitySegments(std::string frameName, const std::vector<ndnrtc::VideoFrameSegment> &segments);

std::vector<boost::shared_ptr<ndn::Interest>>
getInterests(std::string frameName, unsigned int startSeg, size_t nSeg, unsigned int parityStartSeg = 0,
             size_t parityNSeg = 0, unsigned int startNonce = 0x1234);

std::vector<boost::shared_ptr<const ndn::Interest>>
makeInterestsConst(const std::vector<boost::shared_ptr<ndn::Interest>> &interests);

boost::shared_ptr<ndnrtc::WireSegment>
getFakeSegment(std::string threadPrefix, ndnrtc::SampleClass cls, ndnrtc::SegmentClass segCls,
               PacketNumber pNo, unsigned int segNo);

boost::shared_ptr<ndnrtc::WireSegment>
getFakeThreadMetadataSegment(std::string threadPrefix, const ndnrtc::DataPacket &d);

ndn::Name
keyName(std::string s);

ndn::Name
certName(ndn::Name keyName);

boost::shared_ptr<ndn::KeyChain>
memoryKeyChain(const std::string name);

bool checkNfd();

namespace testing
{
namespace internal
{
enum GTestColor
{
    COLOR_DEFAULT,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW
};

extern void ColoredPrintf(GTestColor color, const char *fmt, ...);
}
}

//******************************************************************************
typedef boost::function<void(void)> QueueBlock;

#if BOOST_ASIO_HAS_STD_CHRONO

typedef std::chrono::high_resolution_clock::time_point TPoint;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds Msec;
namespace lib_chrono = std::chrono;

#else

typedef boost::chrono::high_resolution_clock::time_point TPoint;
typedef boost::chrono::high_resolution_clock Clock;
typedef boost::chrono::milliseconds Msec;
namespace lib_chrono = boost::chrono;

#endif

class DelayQueue
{
  public:
    DelayQueue(boost::asio::io_service &io, int delayMs, int deviation = 0) : timer_(io), delayMs_(delayMs), dev_(deviation), timerSet_(false)
    {
        std::srand(std::time(0));
    }
    ~DelayQueue()
    {
    }

    void push(QueueBlock block);
    void reset();
    size_t size() const;
    bool isActive() const { return timerSet_; }

  private:
    mutable boost::recursive_mutex m_;
    boost::atomic<bool> timerSet_;
    int delayMs_, dev_;
    boost::asio::steady_timer timer_;
    std::map<TPoint, std::vector<QueueBlock>> queue_;

    void pop(const boost::system::error_code &e);
};

typedef boost::function<void(const boost::shared_ptr<ndn::Interest> &)> OnInterestT;
typedef boost::function<void(const boost::shared_ptr<ndn::Data> &, const boost::shared_ptr<ndn::Interest>)> OnDataT;

class DataCache
{
  public:
    void addInterest(const boost::shared_ptr<ndn::Interest> interest, OnDataT onData);
    void addData(const boost::shared_ptr<ndn::Data> &data, OnInterestT onInterest = OnInterestT());

  private:
    boost::mutex m_;
    std::map<ndn::Name, boost::shared_ptr<ndn::Interest>> interests_;
    std::map<ndn::Name, OnDataT> onDataCallbacks_;
    std::map<ndn::Name, boost::shared_ptr<ndn::Data>> data_;
    std::map<ndn::Name, OnInterestT> onInterestCallbacks_;
};

#endif
