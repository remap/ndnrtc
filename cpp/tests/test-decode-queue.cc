//
// test-decode-queue.cc
//
//  Created by Peter Gusev on 1 September 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include <cstdlib>
#include <stdlib.h>
#include <iostream>
#include <deque>
#include <thread>

#define BOOST_BIND_NO_PLACEHOLDERS
#include <boost/filesystem.hpp>
#include <boost/asio/steady_timer.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>
#include <ndn-cpp/security/safe-bag.hpp>
#include <ndn-cpp/security/v2/validation-policy-from-pib.hpp>
#include <ndn-cpp/security/v2/validator.hpp>

#include "gtest/gtest.h"
#include "include/ndnrtc-common.hpp"
#include "include/stream.hpp"
#include "include/name-components.hpp"
#include "include/simple-log.hpp"
#include "tests-helpers.hpp"

#include "../src/frame-buffer.hpp"
#include "../src/video-codec.hpp"
#include "../src/fec.hpp"
#include "../src/packets.hpp"
#include "../src/clock.hpp"
#include "../src/proto/ndnrtc.pb.h"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace std;
using namespace std::placeholders;
using namespace std::chrono;
using namespace ndn;

typedef boost::asio::steady_timer timer_type;
typedef std::chrono::milliseconds ms;
typedef std::chrono::nanoseconds ns;
typedef std::chrono::high_resolution_clock::time_point time_point_type;
typedef std::chrono::high_resolution_clock::duration duration_type;

string resources_path = "";
string test_path = "";

typedef struct _FrameStruct {
    vector<shared_ptr<Data>> packets_;
    shared_ptr<BufferSlot> slot_;
} FrameDataStruct;

vector<FrameDataStruct> EncodedFrames;

void prepData();
vector<shared_ptr<Data>> publishFrameGobj(const EncodedFrame&, const Name&,
                                            const PacketNumber&, const uint32_t&,
                                            const uint32_t&);
vector<uint8_t> generateFecData(const EncodedFrame &, size_t, size_t, size_t);
shared_ptr<BufferSlot> prepBufferSlot(const vector<shared_ptr<Data>>& packets);
int readYUV420(FILE *f, int width, int height, uint8_t *buf);

#if 1
TEST(TestDecodeQueue, TestDecodeFwdSmooth)
{
    Logger::initAsyncLogging();
    Logger::getLogger("").setLogLevel(NdnLoggerDetailLevelNone);
//    Logger::getLogger("").setLogLevel(NdnLoggerDetailLevelAll);

    // - instantiate DecodeQueue and push ready slots into it as needed
    boost::asio::io_service io;
    {

        shared_ptr<DecodeQueue> dq = make_shared<DecodeQueue>(io, 90);
        dq->setLogger(Logger::getLoggerPtr(""));
        FILE *fOut = fopen("/tmp/received", "wb");
        int nRetrieved = 0;

        // push one by one
        for (auto& s : EncodedFrames)
        {
            dq->push(s.slot_);
            io.poll();
            io.reset();
            usleep(30000);

            // we don't want to copy image, just use reference
            const VideoCodec::Image& img = dq->get(1);
            EXPECT_EQ(img.getWidth(), 1280);
            EXPECT_EQ(img.getHeight(), 720);
            EXPECT_GT(img.getDataSize(), 0);
            nRetrieved++;

            img.write(fOut);
        }

        EXPECT_EQ(nRetrieved, EncodedFrames.size());
    }
}
#endif
#if 1
TEST(TestDecodeQueue, TestDecodeFwdIrregularPush)
{
    Logger::initAsyncLogging();
    Logger::getLogger("").setLogLevel(NdnLoggerDetailLevelNone);
//    Logger::getLogger("").setLogLevel(NdnLoggerDetailLevelAll);

    // - instantiate DecodeQueue and push ready slots into it as needed
    boost::asio::io_service io;
    {
        int maxQsize = 50;
        shared_ptr<DecodeQueue> dq = make_shared<DecodeQueue>(io, maxQsize);
        dq->setLogger(Logger::getLoggerPtr(""));
        FILE *fOut = fopen("/tmp/received", "wb");

        std::srand(std::time(nullptr));
        int pushInterval = 1000/30, pushIntervalVar = 15;
        int dequeInterval = 1000/30;
        timer_type pushTimer(io);
        timer_type dequeTimer(io);

        int pushedIdx = 0;
        function<void(const boost::system::error_code&)> pushSlot;
        auto setupPushTimer = [&pushTimer, &pushSlot, pushInterval, pushIntervalVar](){
            int interval = int(pushInterval) + (std::rand() % pushIntervalVar - pushIntervalVar/2);
            pushTimer.expires_from_now(ms(interval));
            pushTimer.async_wait(bind(pushSlot, _1));
//            cout << "push in " << interval << "ms" << endl;
        };
        pushSlot = [&pushedIdx, dq, &setupPushTimer](const boost::system::error_code& e){
            if (!e)
            {
                if (pushedIdx < EncodedFrames.size())
                {
                    dq->push(EncodedFrames[pushedIdx++].slot_);
                    setupPushTimer();
                }
                else
                    cout << "pushed all slots" << endl;
            }
        };

        int dequedNum = 0, notReadyNum = 0;
        PacketNumber prevRetrieved = 0;
        function<void(const boost::system::error_code&)> dequeSlot;
        auto setupDequeTimer = [&dequeTimer, &dequeSlot](int interval){
            dequeTimer.expires_from_now(ms(interval));
            dequeTimer.async_wait(bind(dequeSlot, _1));
        };

        bool dequeStarted = false;
        dequeSlot = [&dequedNum, &notReadyNum, dq, fOut, setupDequeTimer,
                     dequeInterval, maxQsize, &prevRetrieved, &dequeStarted]
        (const boost::system::error_code& e)
        {
            if (!e)
            {
                EXPECT_LE(dq->getSize(), maxQsize);
                int32_t seekPos = 0;
                if (dequeStarted)
                    seekPos = dq->seek(1);

                if (seekPos == 1 || (!dequeStarted && dq->getSize()))
                {
                    dequeStarted = true;
                    const VideoCodec::Image& img = dq->get();
                    if (img.getDataSize())
                    {
                        EXPECT_EQ(img.getWidth(), 1280);
                        EXPECT_EQ(img.getHeight(), 720);
                        EXPECT_GT(img.getDataSize(), 0);

                        NamespaceInfo* ni = (NamespaceInfo*)img.getUserData();
                        EXPECT_TRUE(ni);
                        if (ni->sampleNo_)
                            EXPECT_EQ(prevRetrieved+1, ni->sampleNo_);
                        cout << "got frame " << ni->sampleNo_ << endl;
                        prevRetrieved = ni->sampleNo_;

                        dequedNum += 1;
                        cout << "dequed " << dequedNum << endl;
                       img.write(fOut);
                    }
                    else
                        cout << "got zero image" << endl;
                }
                else
                    notReadyNum++;
                setupDequeTimer(dequeInterval);
            }
        };

        setupPushTimer();
        // delay deque by 100ms
        setupDequeTimer(100);

        while (dequedNum < EncodedFrames.size())
        {
            io.run_one();
            io.reset();
        }

        EXPECT_EQ(dequedNum, EncodedFrames.size());
        cout << "dequed " << dequedNum << " not ready " << notReadyNum << endl;
    }
}
#endif
#if 1
TEST(TestDecodeQueue, TestDecodeBckwd)
{
    Logger::initAsyncLogging();
    Logger::getLogger("").setLogLevel(NdnLoggerDetailLevelNone);
//    Logger::getLogger("").setLogLevel(NdnLoggerDetailLevelAll);

    // - instantiate DecodeQueue
    // - push slots when needed by deque -- this is to simulate
    //  playback-driven data fetching, i.e. new data fetching is synchornized
    //  with the data playback
    boost::asio::io_service io;
    boost::asio::io_service playbackIo;
    shared_ptr<boost::asio::io_service::work> playbackW =
        make_shared<boost::asio::io_service::work>(playbackIo);

    {
        int maxQsize = 90;
        shared_ptr<DecodeQueue> dq = make_shared<DecodeQueue>(io, maxQsize);
        dq->setLogger(Logger::getLoggerPtr(""));
        FILE *fOut = fopen("/tmp/received", "wb");

        std::srand(std::time(nullptr));
        int pushInterval = 1000/60, pushIntervalVar = 15;
        int dequeInterval = 1000/30;
        timer_type pushTimer(playbackIo);
        timer_type dequeTimer(playbackIo);

        int pushedIdx = EncodedFrames.size()-1;
        deque<FrameDataStruct> gopBuffer;
        int32_t lastGop = -1;
        auto pushSlot = [&pushedIdx, dq, &gopBuffer, &lastGop]()
        {
            if (pushedIdx >= 0)
            {
                bool isKey = EncodedFrames[pushedIdx].slot_->getMetaPacket()->getFrameMeta().type() == FrameMeta_FrameType_Key;
                // check if keyframe
                if (isKey)
                {
                    // push gop
                    dq->push(EncodedFrames[pushedIdx--].slot_);
                    for (auto& ss:gopBuffer)
                        dq->push(ss.slot_);
                    gopBuffer.clear();
                }
                else
                    gopBuffer.push_front(EncodedFrames[pushedIdx--]);
            }
            else
                cout << "pushed all slots" << endl;
        };

        int dequedNum = 0, notReadyNum = 0;
        PacketNumber prevRetrieved = 0;
        function<void(const boost::system::error_code&)> dequeSlot;
        auto setupDequeTimer = [&dequeTimer, &dequeSlot](int interval){
            dequeTimer.expires_from_now(ms(interval));
            dequeTimer.async_wait(bind(dequeSlot, _1));
        };

        bool dequeStarted = false;
        dequeSlot = [&dequedNum, &notReadyNum, dq, fOut, setupDequeTimer,
                     dequeInterval, maxQsize, &prevRetrieved, &dequeStarted,
                     pushSlot, &playbackIo]
        (const boost::system::error_code& e)
        {
            if (!e)
            {
                int32_t seekPos = 0;

                if (dq->getSize() && !dequeStarted)
                    dq->seek(dq->getSize());
                else
                    seekPos = dq->seek(-1);

                if (seekPos == -1 || (dq->getSize() && !dequeStarted))
                {
                    dequeStarted = true;
                    const VideoCodec::Image& img = dq->get();
                    if (img.getDataSize())
                    {
                        EXPECT_EQ(img.getWidth(), 1280);
                        EXPECT_EQ(img.getHeight(), 720);
                        EXPECT_GT(img.getDataSize(), 0);

                        NamespaceInfo* ni = (NamespaceInfo*)img.getUserData();
                        EXPECT_TRUE(ni);
                        if (prevRetrieved)
                            EXPECT_EQ(prevRetrieved, ni->sampleNo_+1);
                        else
                            EXPECT_EQ(EncodedFrames.size()-1, ni->sampleNo_);
                        prevRetrieved = ni->sampleNo_;

                        dequedNum += 1;
                        img.write(fOut);

                        // fetchahead: push next slot
                        playbackIo.dispatch(pushSlot);
                        // pushSlot();
                    }
                }
                else
                    notReadyNum++;
                if (dequedNum < EncodedFrames.size())
                    setupDequeTimer(dequeInterval);
            }
        };

        // pre-fetch frames
        int fetchAhead = 40;
        for (int i = 0; i < fetchAhead; ++i)
            pushSlot();
        // delay deque by 1000ms -- allow to accumulate at least 1 gop of frames
        setupDequeTimer(2000);

        thread playbackThread([&]{ playbackIo.run(); });
        while (dequedNum < EncodedFrames.size())
        {
            io.run();
            io.reset();
        }
        playbackW.reset();
        playbackThread.join();

        EXPECT_EQ(dequedNum, EncodedFrames.size());
        cout << "dequed " << dequedNum << " not ready " << notReadyNum << endl;
    }
}
#endif

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    #ifdef HAVE_BOOST_FILESYSTEM
        boost::filesystem::path testPath(boost::filesystem::system_complete(argv[0]).remove_filename());
        test_path = testPath.string();

        boost::filesystem::path resPath(testPath);
        #ifndef __ANDROID__
        if (resPath.leaf() == ".libs")
            resPath /= boost::filesystem::path("..");

        resPath /= boost::filesystem::path("..") /=
                   boost::filesystem::path("..") /=
                   boost::filesystem::path("resources");
        #else
        resPath = test_path;
        #endif

        resources_path = resPath.string();
    #else
        test_path = std::string(argv[0]);
        std::vector<std::string> comps;
        boost::split(comps, test_path, boost::is_any_of("/"));

        test_path = "";
        for (int i = 0; i < comps.size()-1; ++i)
        {
            test_path += comps[i];
            if (i != comps.size()-1) test_path += "/";
        }

        #ifndef __ANDROID__
        resources_path = test_path + "../../resources";
        #else
        resources_path = test_path;
        #endif
    #endif

    prepData();
    return RUN_ALL_TESTS();
}

void
prepData()
{
    // - encode video file and save encoded data packets for each frame
    // - for each frame, prep BufferSlot with ready state
    int w = 1280, h = 720;
    string fName = "eb_dog_"+to_string(w)+"x"+to_string(h)+"_240.yuv";
    FILE *fIn = fopen((resources_path+"/eb_samples/"+fName).c_str(), "rb");
    if (!fIn)
        FAIL() << "couldn't open input video file";

    string basePrefix = "/test/base/prefix";
    string streamName = "test-stream";
    Name streamPrefix = NameComponents::streamPrefix(basePrefix, streamName);

    VideoCodec c;
    CodecSettings cs = VideoCodec::defaultCodecSettings();
    cs.spec_.encoder_.width_ = w;
    cs.spec_.encoder_.height_ = h;

    EXPECT_NO_THROW(c.initEncoder(cs));

    int frameIdx = 0, gopNo = -1, gopPos = 0;
    vector<uint8_t> frameBuf(3*w*h/2, 0);
    while (readYUV420(fIn, w, h, frameBuf.data()))
    {
        // cout << "read frame " << frameIdx++ << endl;
        VideoCodec::Image img(w,h,ImageFormat::I420, frameBuf.data());
        c.encode(img, false,
            [&](const EncodedFrame& ef){
                if (ef.type_ == FrameType::Key)
                {
                    gopNo++;
                    gopPos = 0;
                }

                FrameDataStruct s;
                s.packets_ = publishFrameGobj(ef, streamPrefix, frameIdx, gopNo, gopPos);
                s.slot_ = prepBufferSlot(s.packets_);
                EncodedFrames.push_back(s);

                EXPECT_EQ(s.slot_->getState(), PipelineSlotState::Ready);
                EXPECT_TRUE(s.slot_->isReadyForDecoder());

                frameIdx++;
                gopPos++;
            },
            [](const VideoCodec::Image& img){
                FAIL() << "encoder shouldn't drop frames";
            });
    }
}

vector<shared_ptr<Data>>
publishFrameGobj(const EncodedFrame& frame,
    const Name& streamPrefix, const PacketNumber& sampleNo,
    const uint32_t& gopNo, const uint32_t& gopPos)
{
    vector<shared_ptr<Data>> packets;
    Name frameName = Name(streamPrefix).appendSequenceNumber(sampleNo);
    FrameType ft = frame.type_;
    size_t segSize = 8000;
    double parityRatio = 0.2;
    size_t nDataSegments = frame.length_ / segSize + (frame.length_ % segSize ? 1 : 0);
    size_t nParitySegments = ceil(parityRatio * nDataSegments);
    Name::Component dataFinalBlockId = Name::Component::fromSegment(nDataSegments-1);
    Name::Component parityFinalBlockId = Name::Component::fromSegment(nParitySegments-1);
    vector<uint8_t> fecData = move(generateFecData(frame, nDataSegments,
        nParitySegments, segSize));

    EXPECT_GT(fecData.size(), 0);

    uint8_t *slice = frame.data_;
    size_t lastSegSize = frame.length_ - segSize * (nDataSegments-1);

    for (int seg = 0; seg < nDataSegments; ++seg)
    {
        // TODO: explore NDN-CPP Lite API for zerocopy
        std::shared_ptr<Data> d = std::make_shared<Data>(Name(frameName).appendSegment(seg));
        d->getMetaInfo().setFreshnessPeriod(1000);
        d->getMetaInfo().setFinalBlockId(dataFinalBlockId);
        d->setContent(slice, (seg == nDataSegments-1 ? lastSegSize : segSize));

        packets.push_back(d);
        slice += segSize;
    }

    slice = fecData.data();
    for (int seg = 0; seg < nParitySegments; ++seg)
    {
        std::shared_ptr<Data> d = std::make_shared<Data>(Name(frameName)
            .append(NameComponents::Parity).appendSegment(seg));
        d->getMetaInfo().setFreshnessPeriod(1000);
        d->getMetaInfo().setFinalBlockId(parityFinalBlockId);
        d->setContent(slice, segSize);

        packets.push_back(d);
        slice += segSize;
    }

    shared_ptr<Data> manifest =
        SegmentsManifest::packManifest(Name(frameName).append(NameComponents::Manifest),
            packets);
    packets.push_back(manifest);

    { // prep _meta
        uint64_t now = clock::nanosecondTimestamp();
        int64_t secs = now/1E9;
        int32_t nanosec = now - secs*1E9;
        google::protobuf::Timestamp *captureTimestamp = new google::protobuf::Timestamp();
        captureTimestamp->set_seconds(secs);
        captureTimestamp->set_nanos(nanosec);

        FrameMeta meta;
        meta.set_allocated_capture_timestamp(captureTimestamp);
        meta.set_dataseg_num(nDataSegments);
        meta.set_parity_size(nParitySegments);
        meta.set_gop_number(gopNo);
        meta.set_gop_position(gopPos);
        meta.set_type(ft == FrameType::Key ? FrameMeta_FrameType_Key : FrameMeta_FrameType_Delta);
        meta.set_generation_delay(0);

        std::shared_ptr<Data> d = std::make_shared<Data>(Name(frameName).append(NameComponents::Meta));

        string metaPayload = meta.SerializeAsString();;

        ndntools::ContentMetaInfo metaInfo;
        metaInfo.setContentType("ndnrtcv4")
            .setTimestamp(clock::unixTimestamp())
            .setHasSegments(true)
            .setOther(Blob::fromRawStr(metaPayload));

        d->setContent(metaInfo.wireEncode());
        packets.push_back(d);
    }

    return packets;
}

vector<uint8_t>
generateFecData(const EncodedFrame &frame,
                size_t nDataSegments, size_t nParitySegments,
                size_t payloadSegSize)
{
    vector<uint8_t> fecData(nParitySegments * payloadSegSize, 0);
    fec::Rs28Encoder enc(nDataSegments, nParitySegments, payloadSegSize);
    size_t zeroPadding = nDataSegments * payloadSegSize - frame.length_;

    vector<uint8_t> paddedData(nDataSegments*payloadSegSize, 0);
    copy(frame.data_, frame.data_+frame.length_, paddedData.begin());

    if (enc.encode(paddedData.data(), fecData.data()) != 0)
        fecData.resize(0);

    return fecData;
}

shared_ptr<BufferSlot>
prepBufferSlot(const vector<shared_ptr<Data>>& packets)
{
    shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
    auto expressRequests = [](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &r:requests)
        {
            p.timestampRequest(*r);
            p.setStatus(DataRequest::Status::Expressed, *r);
            p.triggerEvent(DataRequest::Status::Expressed, *r);
        }
    };
    auto replyData = [&packets](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &d:packets)
        {
            for (auto &r:requests)
            {
                if (d->getName().match(r->getInterest()->getName()))
                {
                    p.timestampReply(*r);
                    p.setData(d, *r);
                    p.setStatus(DataRequest::Status::Data, *r);
                    p.triggerEvent(DataRequest::Status::Data, *r);
                }
            }
        }
    };

    // make requests
    vector<shared_ptr<DataRequest>> requests;
    for (auto& d : packets)
    {
        shared_ptr<Interest> i = make_shared<Interest>(d->getName(), 30);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    slot->setRequests(requests);
    expressRequests(requests);
    replyData(requests);

    return slot;
}

int
readYUV420(FILE *f, int width, int height, uint8_t *imgData)
{
    size_t len = 3*width*height/2;
    uint8_t *buf = imgData;

    for (int plane = 0; plane < 3; ++plane) {
        const int stride = plane == 0 ? width : width/2;
        const int w = plane == 0 ? width : (width+1) >> 1;
        const int h = plane == 0 ? height : (height+1) >> 1;

        for (int y = 0; y < h; ++y)
        {
            if (fread(buf, 1, w, f) != (size_t)w) return 0;
            buf += stride;
        }
    }

    return 1;
}
