//
// test-local-media-stream.cc
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <cstdlib>
#include <ctime>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <boost/thread.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "include/local-stream.hpp"
#include "include/name-components.hpp"

// #define ENABLE_LOGGING

#if BOOST_ASIO_HAS_STD_CHRONO

namespace lib_chrono = std::chrono;

#else

namespace lib_chrono = boost::chrono;

#endif

using namespace ndnrtc;
using namespace ndn;
using namespace boost;
using namespace lib_chrono;

MediaStreamParams getSampleVideoParams()
{
    MediaStreamParams msp("camera");

    msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
    msp.synchronizedStreamName_ = "mic";
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 10;
    msp.captureDevice_ = cdp;

    VideoThreadParams atp("low", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 640;
    atp.coderParams_.encodeHeight_ = 480;
    atp.coderParams_.startBitrate_ = 500;
    msp.addMediaThread(atp);

    VideoThreadParams atp2("mid", sampleVideoCoderParams());
    atp2.coderParams_.encodeWidth_ = 1280;
    atp2.coderParams_.encodeHeight_ = 720;
    atp2.coderParams_.startBitrate_ = 1000;
    msp.addMediaThread(atp2);

    return msp;
}

MediaStreamParams getSampleAudioParams()
{
    MediaStreamParams msp("mic");

    msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
    msp.producerParams_.freshness_ = {10, 15, 900};
    msp.producerParams_.segmentSize_ = 1000;

    CaptureDeviceParams cdp;
    cdp.deviceId_ = 0;
    msp.captureDevice_ = cdp;
    msp.addMediaThread(AudioThreadParams("hd", "opus"));
    msp.addMediaThread(AudioThreadParams("sd", "g722"));

    return msp;
}
#if 1
TEST(TestVideoStream, TestCreate)
{
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    ndn::KeyChain keyChain;
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = &keyChain;
    std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";
    LocalVideoStream s(streamPrefix, settings);

    ASSERT_EQ(2, s.getThreads().size());
    EXPECT_EQ("low", s.getThreads()[0]);
    EXPECT_EQ("mid", s.getThreads()[1]);
    std::stringstream apiV;
    apiV << "\%FD\%" << std::setw(2) << std::setfill('0') << NameComponents::nameApiVersion();

    EXPECT_EQ(streamPrefix + "/ndnrtc/" + apiV.str() + "/video/camera", s.getPrefix());
}

TEST(TestVideoStream, TestParamsError)
{
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    ndn::KeyChain keyChain;
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = &keyChain;
    std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

    settings.params_.getVideoThread(1)->threadName_ = settings.params_.getVideoThread(0)->threadName_;

    EXPECT_ANY_THROW(LocalVideoStream s(streamPrefix, settings));
}

TEST(TestVideoStream, TestAddExistingThread)
{
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    ndn::KeyChain keyChain;
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = &keyChain;
    std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

    LocalVideoStream s(streamPrefix, settings);
    EXPECT_ANY_THROW(s.addThread(*settings.params_.getVideoThread(0)));
}

TEST(TestVideoStream, TestCreateWrongMediaSettings)
{
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    ndn::KeyChain keyChain;
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = &keyChain;
    std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

    settings.params_.type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeAudio;

    EXPECT_ANY_THROW(LocalVideoStream s(streamPrefix, settings));
}

TEST(TestVideoStream, TestDeleteThreads)
{
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    ndn::KeyChain keyChain;
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = &keyChain;
    std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

    LocalVideoStream s(streamPrefix, settings);

    EXPECT_EQ(2, s.getThreads().size());
    EXPECT_NO_THROW(s.removeThread("wrong-thread"));
    EXPECT_EQ(2, s.getThreads().size());

    s.removeThread(settings.params_.getVideoThread(0)->threadName_);
    EXPECT_EQ(1, s.getThreads().size());

    s.removeThread(settings.params_.getVideoThread(1)->threadName_);
    EXPECT_EQ(0, s.getThreads().size());

    // test feeding frame when there are no threads
    int width = 1280, height = 720;
    int frameSize = width * height * 4 * sizeof(uint8_t);
    uint8_t *frameBuffer = (uint8_t *)malloc(frameSize);
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i)
            frameBuffer[i * width + j] = std::rand() % 256; // random noise

    EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frameBuffer, frameSize));
}

TEST(TestVideoStream, TestDestructorNotLocking)
{
    boost::atomic<bool> destructed(false);

    boost::asio::io_service checkIo;
    boost::asio::steady_timer checkTimer(checkIo);

    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

    checkTimer.expires_from_now(lib_chrono::milliseconds(2500));
    checkTimer.async_wait([&destructed, &t](const boost::system::error_code &e) {
        ASSERT_TRUE(destructed);
        if (!destructed)
            t.interrupt();
    });

    boost::thread checkThread([&checkIo]() {
        checkIo.run();
    });

    {
        ndn::Face face("aleph.ndn.ucla.edu");
        shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
        std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
        shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

        MediaStreamSettings settings(io, getSampleVideoParams());
        settings.face_ = &face;
        settings.keyChain_ = keyChain.get();
        LocalVideoStream s(appPrefix, settings);

        work.reset();
        t.timed_join(boost::posix_time::seconds(2));
    }
    destructed = true;
    checkThread.join();
}

TEST(TestVideoStream, TestPublish)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    int width = 1280, height = 720;
    int frameSize = width * height * 4 * sizeof(uint8_t);
    uint8_t *frameBuffer = (uint8_t *)malloc(frameSize);
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i)
            frameBuffer[i * width + j] = std::rand() % 256; // random noise

    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = keyChain.get();
    LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
    s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frameBuffer, frameSize));

    work.reset();
    t.join();
    free(frameBuffer);
}

TEST(TestVideoStream, TestPublishInvokeOnMainThread)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    int nFrames = 150;
    int width = 1280, height = 720;
    std::srand(std::time(0));
    int frameSize = width * height * 4 * sizeof(uint8_t);
    std::vector<boost::shared_ptr<uint8_t>> frames;

    uint8_t *frameBuffer = new uint8_t[frameSize];
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i)
            frameBuffer[i * width + j] = std::rand() % 256; // random noise

    frames.push_back(boost::shared_ptr<uint8_t>(frameBuffer));

    for (int f = 1; f < nFrames; ++f)
    {
        // add noise in rectangle in the center of frame
        uint8_t *buf = new uint8_t[frameSize];
        memcpy(buf, frameBuffer, frameSize);

        for (int j = height / 3; j < 2 * height / 3; ++j)
            for (int i = width / 3; i < 2 * width / 3; ++i)
                frameBuffer[i * width + j] = std::rand() % 256; // random noise

        frames.push_back(boost::shared_ptr<uint8_t>(buf));
    }

    boost::asio::io_service io;
    boost::asio::deadline_timer runTimer(io);
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = keyChain.get();
    LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
    s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
    lib_chrono::duration<int, std::nano> pubDuration;
    high_resolution_clock::time_point pubStart;
    for (int i = 0; i < nFrames; ++i)
    {
        runTimer.expires_from_now(boost::posix_time::milliseconds(30));
        pubStart = high_resolution_clock::now();
        EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frames[i].get(), frameSize));
        auto p = high_resolution_clock::now() - pubStart;
        pubDuration += p;
        runTimer.wait();
    }

    double avgFramePubTimeMs = (double)duration_cast<milliseconds>(pubDuration).count() / (double)nFrames;
    std::stringstream ss;
    for (int i = 0; i < settings.params_.getThreadNum(); ++i)
        ss << settings.params_.getVideoThread(i)->coderParams_.encodeWidth_ << "x"
           << settings.params_.getVideoThread(i)->coderParams_.encodeHeight_ << " "
           << settings.params_.getVideoThread(i)->coderParams_.startBitrate_ << "kBit/s ";

    GT_PRINTF("Total publish time %ld ms\n", duration_cast<milliseconds>(pubDuration).count());
    GT_PRINTF("Published %d frames. Number of threads: %d (%s). Average publishing time is %.2fms\n",
              nFrames, settings.params_.getThreadNum(), ss.str().c_str(), avgFramePubTimeMs);
    EXPECT_GE(1000 / 30, avgFramePubTimeMs);

    work.reset();
    t.join();
}

TEST(TestVideoStream, TestPublishInvokeOnFaceThread)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    int nFrames = 30 * 5;
    int width = 1280;
    int height = 720;
    std::srand(std::time(0));
    int frameSize = width * height * 4 * sizeof(uint8_t);
    std::vector<boost::shared_ptr<uint8_t>> frames;

    uint8_t *frameBuffer = new uint8_t[frameSize];
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i)
            frameBuffer[i * width + j] = std::rand() % 256; // random noise

    frames.push_back(boost::shared_ptr<uint8_t>(frameBuffer));

    for (int f = 1; f < nFrames; ++f)
    {
        // add noise in rectangle in the center of frame
        uint8_t *buf = new uint8_t[frameSize];
        memcpy(buf, frameBuffer, frameSize);

        for (int j = height / 3; j < 2 * height / 3; ++j)
            for (int i = width / 3; i < 2 * width / 3; ++i)
                frameBuffer[i * width + j] = std::rand() % 256; // random noise

        frames.push_back(boost::shared_ptr<uint8_t>(buf));
    }

    boost::asio::io_service io;
    boost::asio::deadline_timer runTimer(io);
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

    {
        MediaStreamSettings settings(io, getSampleVideoParams());
        settings.face_ = &face;
        settings.keyChain_ = keyChain.get();
        LocalVideoStream s(appPrefix, settings);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(3000));

#ifdef ENABLE_LOGGING
        s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
        boost::mutex m;
        boost::unique_lock<boost::mutex> lock(m);
        boost::condition_variable isDone;

        io.dispatch([&settings, frameSize, nFrames, &runTimer, width, height, &s, &frames, &isDone]() {
            lib_chrono::duration<int, std::nano> pubDuration;
            high_resolution_clock::time_point pubStart;
            for (int i = 0; i < nFrames; ++i)
            {
                runTimer.expires_from_now(boost::posix_time::milliseconds(30));

                pubStart = high_resolution_clock::now();
                EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frames[i].get(), frameSize));
                pubDuration += (high_resolution_clock::now() - pubStart);

                runTimer.wait();
            }

            double avgFramePubTimeMs = (double)duration_cast<milliseconds>(pubDuration).count() / (double)nFrames;
            std::stringstream ss;
            for (int i = 0; i < settings.params_.getThreadNum(); ++i)
                ss << settings.params_.getVideoThread(i)->coderParams_.encodeWidth_ << "x"
                   << settings.params_.getVideoThread(i)->coderParams_.encodeHeight_ << " "
                   << settings.params_.getVideoThread(i)->coderParams_.startBitrate_ << "kBit/s ";

            GT_PRINTF("Published %d frames. Number of threads: %d (%s). Average publishing time is %.2fms\n",
                      nFrames, settings.params_.getThreadNum(), ss.str().c_str(), avgFramePubTimeMs);

            // EXPECT_GE(1000/30, avgFramePubTimeMs);
            isDone.notify_one();
        });

        isDone.wait(lock);
    }

    work.reset();
    t.join();
}

TEST(TestVideoStream, TestRemoveThreadWhilePublishing)
{
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    int nFrames = 3 * 30;
    int width = 1280, height = 720;
    std::srand(std::time(0));
    int frameSize = width * height * 4 * sizeof(uint8_t);
    std::vector<boost::shared_ptr<uint8_t>> frames;

    uint8_t *frameBuffer = new uint8_t[frameSize];
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i)
            frameBuffer[i * width + j] = std::rand() % 256; // random noise

    frames.push_back(boost::shared_ptr<uint8_t>(frameBuffer));

    for (int f = 1; f < nFrames; ++f)
    {
        // add noise in rectangle in the center of frame
        uint8_t *buf = new uint8_t[frameSize];
        memcpy(buf, frameBuffer, frameSize);

        for (int j = height / 3; j < 2 * height / 3; ++j)
            for (int i = width / 3; i < 2 * width / 3; ++i)
                frameBuffer[i * width + j] = std::rand() % 256; // random noise

        frames.push_back(boost::shared_ptr<uint8_t>(buf));
    }

    boost::asio::io_service io;
    boost::asio::deadline_timer runTimer(io);
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

    {
        MediaStreamSettings settings(io, getSampleVideoParams());
        settings.face_ = &face;
        settings.keyChain_ = keyChain.get();
        LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
        s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
        lib_chrono::duration<int, std::nano> pubDuration;
        high_resolution_clock::time_point pubStart;
        for (int i = 0; i < nFrames; ++i)
        {
            runTimer.expires_from_now(boost::posix_time::milliseconds(30));
            pubStart = high_resolution_clock::now();

            EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frames[i].get(), frameSize));

            if (i == nFrames / 2)
                s.removeThread(s.getThreads().front());

            pubDuration += (high_resolution_clock::now() - pubStart);
            runTimer.wait();
        }

        double avgFramePubTimeMs = (double)duration_cast<milliseconds>(pubDuration).count() / (double)nFrames;
        EXPECT_GE(1000 / 30, avgFramePubTimeMs);

        work.reset();
    }
    t.join();
}
#endif

#if 1
TEST(TestAudioStream, TestCreate)
{
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    MediaStreamSettings settings(io, getSampleAudioParams());
    settings.face_ = &face;
    settings.keyChain_ = keyChain.get();
    LocalAudioStream s(appPrefix, settings);

    EXPECT_FALSE(s.isRunning());
    ASSERT_EQ(2, s.getThreads().size());
    EXPECT_EQ("hd", s.getThreads()[0]);
    EXPECT_EQ("sd", s.getThreads()[1]);
    std::stringstream apiV;
    apiV << "\%FD\%" << std::setw(2) << std::setfill('0') << NameComponents::nameApiVersion();

    EXPECT_EQ(appPrefix + "/ndnrtc/" + apiV.str() + "/audio/mic", s.getPrefix());
}

TEST(TestAudioStream, TestCreateWrongMediaSettings)
{
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    ndn::KeyChain keyChain;
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = &face;
    settings.keyChain_ = &keyChain;
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    EXPECT_ANY_THROW(LocalAudioStream(appPrefix, settings));
}

TEST(TestAudioStream, TestAddRemoveThreads)
{
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    MediaStreamSettings settings(io, getSampleAudioParams());
    settings.face_ = &face;
    settings.keyChain_ = keyChain.get();
    LocalAudioStream s(appPrefix, settings);

    ASSERT_EQ(2, s.getThreads().size());
    EXPECT_EQ("hd", s.getThreads()[0]);
    EXPECT_EQ("sd", s.getThreads()[1]);
    std::stringstream apiV;
    apiV << "\%FD\%" << std::setw(2) << std::setfill('0') << NameComponents::nameApiVersion();

    EXPECT_EQ(appPrefix + "/ndnrtc/" + apiV.str() + "/audio/mic", s.getPrefix());

    EXPECT_NO_THROW(s.addThread(AudioThreadParams("sd2")));
    EXPECT_EQ(3, s.getThreads().size());
    EXPECT_ANY_THROW(s.addThread(AudioThreadParams("sd2")));

    EXPECT_NO_THROW(s.removeThread("sd"));
    EXPECT_EQ(2, s.getThreads().size());
    EXPECT_NO_THROW(s.removeThread("hd"));
    EXPECT_EQ(1, s.getThreads().size());
    EXPECT_NO_THROW(s.removeThread("sd2"));
    EXPECT_EQ(0, s.getThreads().size());

    EXPECT_NO_THROW(s.removeThread("sd"));
    EXPECT_EQ(0, s.getThreads().size());
}

TEST(TestAudioStream, TestRun5Sec)
{
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::asio::deadline_timer runTimer(io);
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
    {
        MediaStreamSettings settings(io, getSampleAudioParams());
        settings.face_ = &face;
        settings.keyChain_ = keyChain.get();
        LocalAudioStream s(appPrefix, settings);
        EXPECT_FALSE(s.isRunning());

#ifdef ENABLE_LOGGING
        s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

        runTimer.expires_from_now(boost::posix_time::milliseconds(5000));
        EXPECT_NO_THROW(s.start());
        EXPECT_TRUE(s.isRunning());

        runTimer.wait();
        EXPECT_NO_THROW(s.stop());
        EXPECT_FALSE(s.isRunning());
    }

    work.reset();
    t.join();
}

TEST(TestAudioStream, TestRunAndStopOnDtor)
{
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::asio::deadline_timer runTimer(io);
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
    {
        MediaStreamSettings settings(io, getSampleAudioParams());
        settings.face_ = &face;
        settings.keyChain_ = keyChain.get();
        LocalAudioStream s(appPrefix, settings);
        EXPECT_FALSE(s.isRunning());

#ifdef ENABLE_LOGGING
        s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

        runTimer.expires_from_now(boost::posix_time::milliseconds(500));
        EXPECT_NO_THROW(s.start());
        EXPECT_TRUE(s.isRunning());

        runTimer.wait();
    }

    work.reset();
    t.join();
}

TEST(TestAudioStream, TestAddRemoveThreadsOnTheFly)
{
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::asio::io_service io;
    ndn::Face face("aleph.ndn.ucla.edu");
    shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::asio::deadline_timer runTimer(io);
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io]() {
        io.run();
    });

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
    {
        MediaStreamParams msp("mic");
        msp.producerParams_.segmentSize_ = 1000;
        msp.producerParams_.freshness_ = {10, 15, 900};
        msp.captureDevice_.deviceId_ = 0;
        msp.type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeAudio;
        msp.addMediaThread(AudioThreadParams("sd"));
        MediaStreamSettings settings(io, msp);
        settings.face_ = &face;
        settings.keyChain_ = keyChain.get();
        LocalAudioStream s(appPrefix, settings);
        EXPECT_FALSE(s.isRunning());

#ifdef ENABLE_LOGGING
        s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

        runTimer.expires_from_now(boost::posix_time::milliseconds(1000));
        EXPECT_NO_THROW(s.start());
        EXPECT_TRUE(s.isRunning());

        runTimer.wait();
        int n = 3;
        std::srand(std::time(0));

        for (int i = 0; i < n; ++i)
        {
            GT_PRINTF("thread added...\n");
            EXPECT_NO_THROW(s.addThread(AudioThreadParams("hd", "opus")));
            EXPECT_TRUE(s.isRunning());
            runTimer.expires_from_now(boost::posix_time::milliseconds(std::rand() % 1000 + 500));
            runTimer.wait();

            GT_PRINTF("thread removed %d/%d\n", i + 1, n);
            EXPECT_NO_THROW(s.removeThread("hd"));
            EXPECT_TRUE(s.isRunning());
            runTimer.expires_from_now(boost::posix_time::milliseconds(std::rand() % 1000 + 500));
            runTimer.wait();
        }

        EXPECT_NO_THROW(s.removeThread("sd"));
        EXPECT_FALSE(s.isRunning());
    }

    work.reset();
    t.join();
}
#endif

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
