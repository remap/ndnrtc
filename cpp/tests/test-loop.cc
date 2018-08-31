//
// test-loop.cc
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <execinfo.h>

#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/transport/tcp-transport.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/filesystem.hpp>

#include "gtest/gtest.h"
#include "remote-stream.hpp"
#include "local-stream.hpp"
#include "tests-helpers.hpp"
#include "client/src/video-source.hpp"
#include "client/src/frame-io.hpp"
#include "estimators.hpp"

#include "mock-objects/external-capturer-mock.hpp"
#include "mock-objects/external-renderer-mock.hpp"

#define ENABLE_LOGGING
// #define SAVE_VIDEO

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

std::string test_path = "";
std::string resources_path = "";
std::string logs_folder = "logs";
std::string logs_path = ".";

MediaStreamParams getSampleVideoParams()
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshness_ = { 10, 15, 900 };
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("low", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 320;
	atp.coderParams_.encodeHeight_ = 240;
	msp.addMediaThread(atp);

//   {
//     VideoThreadParams atp("mid", sampleVideoCoderParams());
//     atp.coderParams_.encodeWidth_ = 320;
//     atp.coderParams_.encodeHeight_ = 240;
//     msp.addMediaThread(atp);
//   }

	return msp;
}

boost::shared_ptr<KeyChain> memoryKeyChain2(const std::string name)
{
    boost::shared_ptr<MemoryIdentityStorage> identityStorage(new MemoryIdentityStorage());
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(new MemoryPrivateKeyStorage());
    boost::shared_ptr<KeyChain> keyChain(boost::make_shared<KeyChain>
      (boost::make_shared<IdentityManager>(identityStorage, privateKeyStorage),
       boost::make_shared<SelfVerifyPolicyManager>(identityStorage.get())));

    // Initialize the storage.

    Name kn = keyChain->generateRSAKeyPairAsDefault(Name(name));
    // Name certificateName = keyChain->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(name));

    // identityStorage->addKey(kn, KEY_TYPE_RSA, Blob(DEFAULT_RSA_PUBLIC_KEY_DER, sizeof(DEFAULT_RSA_PUBLIC_KEY_DER)));
    // privateKeyStorage->setKeyPairForKeyName
    //   (kn, KEY_TYPE_RSA, DEFAULT_RSA_PUBLIC_KEY_DER,
    //    sizeof(DEFAULT_RSA_PUBLIC_KEY_DER), DEFAULT_RSA_PRIVATE_KEY_DER,
    //    sizeof(DEFAULT_RSA_PRIVATE_KEY_DER));

    return keyChain;
}

MediaStreamParams getSampleAudioParams()
{
	MediaStreamParams msp("mic");

	msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
	msp.producerParams_.freshness_ = { 10, 15, 900 };
	msp.producerParams_.segmentSize_ = 1000;
	
	CaptureDeviceParams cdp;
	cdp.deviceId_ = 0;
	msp.captureDevice_ = cdp;
	msp.addMediaThread(AudioThreadParams("hd", "opus"));
	msp.addMediaThread(AudioThreadParams("sd", "g722"));

	return msp;
}

std::string createUnitTestFolder(const std::vector<std::string> &comps)
{
#ifdef HAVE_BOOST_FILESYSTEM
    boost::filesystem::path path;

    int idx = 0;
    for (auto c:comps)
    {
        path /= c;
        if (!boost::filesystem::exists(path))
            if (!boost::filesystem::create_directory(path))
                GT_PRINTF("Failed to create log directory at %s\n", path.string().c_str());
    }

    return path.string();
#else
    return ".";
#endif
}

TEST(TestLoop, TestVideo)
{
    if (!checkNfd()) return;

#ifdef ENABLE_LOGGING
    std::string testCaseLogsFolder = createUnitTestFolder({ logs_path, 
                                                            ::testing::UnitTest::GetInstance()->current_test_info()->name(), 
                                                            ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name() });
    std::string remoteStreamLoggerPath = testCaseLogsFolder + "/" + "consumer-loop.log";
    std::string localStreamLoggerPath = testCaseLogsFolder + "/" + "producer-loop.log";
    std::string statLoggerPath = testCaseLogsFolder + "/" + "stats.log";

    GT_PRINTF("For this test, see logs at %s\n", testCaseLogsFolder.c_str());

    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger(remoteStreamLoggerPath).setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
    ndnlog::new_api::Logger::getLogger(localStreamLoggerPath).setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
    
    boost::asio::io_service io_source;
    boost::shared_ptr<boost::asio::io_service::work> work_source(boost::make_shared<boost::asio::io_service::work>(io_source));
    boost::thread t_source([&io_source](){
        io_source.run();
    });
    
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){
        io.run();
    });
    
    // video source
    boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(320,240));
    std::string testVideoSource = resources_path+"/test-source-320x240.argb";
    VideoSource source(io_source, testVideoSource, frame);
    MockExternalCapturer capturer;
    MockExternalRenderer renderer;
    source.addCapturer(&capturer);
    
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<Face> consumerFace(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    publisherFace->registerPrefix(Name(appPrefix), OnInterestCallback(),
                                  [](const boost::shared_ptr<const Name>&){
                                      ASSERT_FALSE(true);
                                  });
    // making sure that prefix registration gets through
    boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));

    boost::atomic<bool> done(false);
    boost::asio::deadline_timer statTimer(io);
    boost::function<void(const boost::system::error_code&)> queryStat;
    boost::function<void()> setupTimer = [&statTimer, &queryStat, &done](){
      if (!done)
      {
        statTimer.expires_from_now(boost::posix_time::milliseconds(10));
        statTimer.async_wait(queryStat);
      }
    };

    int rebufferingsNum = 0, nRendered = 0, state = 0;
    estimators::Average bufferLevel(boost::make_shared<estimators::SampleWindow>(10));
    {
      MediaStreamSettings settings(io, getSampleVideoParams());
      settings.face_ = publisherFace.get();
      settings.keyChain_ = keyChain.get();
      LocalVideoStream localStream(appPrefix, settings);
      
#ifdef ENABLE_LOGGING
      localStream.setLogger(ndnlog::new_api::Logger::getLoggerPtr(localStreamLoggerPath));
#endif
      
      boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
      incomingRawFrame =[&localStream](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
          EXPECT_NO_THROW(localStream.incomingArgbFrame(w, h, data, size));
          return 0;
      };
      
      EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
        .WillRepeatedly(Invoke(incomingRawFrame));
      
      int runTime = 10000;
      
      keyChain->setFace(consumerFace.get());
      RemoteVideoStream rs(io, consumerFace, keyChain, appPrefix, getSampleVideoParams().streamName_);

      queryStat = [&statTimer, &rs, &setupTimer, &rebufferingsNum, &bufferLevel, &state, statLoggerPath]
      (const boost::system::error_code& err)
      {
        if (err == boost::asio::error::operation_aborted)
          return;

        StatisticsStorage storage = rs.getStatistics();
        rebufferingsNum = storage[Indicator::RebufferingsNum];
        
        if (storage[Indicator::State] > state)
          state = storage[Indicator::State];

        // EXPECT_EQ(0, storage[Indicator::IncompleteNum]);
        bufferLevel.newValue(storage[Indicator::BufferPlayableSize]);

        LogInfo(statLoggerPath)
            << storage[Indicator::LatencyEstimated]
            << "\t" << storage[Indicator::DrdOriginalEstimation]
            << "\t" << storage[Indicator::DrdCachedEstimation]
            << "\t" << storage[Indicator::BufferTargetSize]
            << "\t" << storage[Indicator::BufferPlayableSize]
            << std::endl;

        setupTimer();
      };
      setupTimer();
      
#ifdef ENABLE_LOGGING
      rs.setLogger(ndnlog::new_api::Logger::getLoggerPtr(remoteStreamLoggerPath));
#endif
      
      EXPECT_CALL(renderer, getFrameBuffer(320,240))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(frame->getBuffer().get()));
      
#ifdef SAVE_VIDEO
      FileSink frameSink("/tmp/frame-sink.argb");
#endif
      boost::function<void(const FrameInfo&,int,int,const uint8_t*)> 
#ifdef SAVE_VIDEO
        renderFrame = [&nRendered, &frameSink, frame](int64_t,uint,int,int,const uint8_t* buf){
#else
        renderFrame = [&nRendered, frame](const FrameInfo&,int,int,const uint8_t* buf){
#endif
          nRendered++;
          EXPECT_EQ(frame->getBuffer().get(), buf);
#ifdef SAVE_VIDEO
          frameSink << *frame;
#endif
        };
      EXPECT_CALL(renderer, renderBGRAFrame(_,_,_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(renderFrame));

      GT_PRINTF("Started publishing stream\n");
      source.start(30);

      int waitThreads = 0;
      while (rs.getThreads().size() == 0 && waitThreads++ < 5)
          boost::this_thread::sleep_for(boost::chrono::milliseconds(1500));
      ASSERT_LT(0, rs.getThreads().size());

      // wait random milliseconds before fetching
      int waitRandom = rand()%5000 + 1000;
      GT_PRINTF("Waiting %dms before initiating fetching\n", waitRandom);
      boost::this_thread::sleep_for(boost::chrono::milliseconds(waitRandom));

      rs.start(rs.getThreads()[0], &renderer);
      boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
      done = true;
      rs.stop();
      source.stop();
      statTimer.cancel();
    }

    work_source.reset();
    t_source.join();
    io_source.stop();
    
    io.dispatch([consumerFace, publisherFace]{
      consumerFace->shutdown();
      publisherFace->shutdown();
    });
    work.reset();
    t.join();
    io.stop();

    GT_PRINTF("Rebufferins: %d, Buffer Level: %.2f, Rendered frames: %d\n",
      rebufferingsNum, bufferLevel.value(), nRendered);

    ASSERT_EQ(0, rebufferingsNum);
    EXPECT_GE(state, 5);
    EXPECT_LT(110, bufferLevel.value());
    EXPECT_GT(200, bufferLevel.value());
}
#if 0
TEST(TestLoop, TestAudio)
{
    if (!checkNfd()) return;
    
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
    
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&io](){
        io.run();
    });
    
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<Face> consumerFace(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    publisherFace->registerPrefix(Name(appPrefix), OnInterestCallback(),
                                  [](const boost::shared_ptr<const Name>&){
                                      ASSERT_FALSE(true);
                                  });
    // making sure that prefix registration gets through
    boost::this_thread::sleep_for(boost::chrono::milliseconds(2000));

    boost::atomic<bool> done(false);
    boost::asio::deadline_timer statTimer(io);
    boost::function<void(const boost::system::error_code&)> queryStat;
    boost::function<void()> setupTimer = [&statTimer, &queryStat, &done](){
        if (!done)
        {
            statTimer.expires_from_now(boost::posix_time::milliseconds(10));
            statTimer.async_wait(queryStat);
        }
    };
    
    int rebufferingsNum = 0, state = 0;
    estimators::Average bufferLevel(boost::make_shared<estimators::SampleWindow>(10));
    {
        MediaStreamSettings settings(io, getSampleAudioParams());
        settings.face_ = publisherFace.get();
        settings.keyChain_ = keyChain.get();
        LocalAudioStream s(appPrefix, settings);
        RemoteAudioStream remoteStream(io, consumerFace, keyChain,
                                       appPrefix, getSampleAudioParams().streamName_);

#ifdef ENABLE_LOGGING
//        s.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
        remoteStream.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif
        s.start();
        s.isRunning();

        int waitThreads = 0;
        while (remoteStream.getThreads().size() == 0 && waitThreads++ < 3)
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1500));
        ASSERT_LT(0, remoteStream.getThreads().size());
        remoteStream.start(remoteStream.getThreads()[0]);
        
        queryStat = [&statTimer, &remoteStream, &setupTimer, &rebufferingsNum, &bufferLevel, &state](const boost::system::error_code& err){
          if (err == boost::asio::error::operation_aborted)
            return;

          StatisticsStorage storage = remoteStream.getStatistics();
          rebufferingsNum = storage[Indicator::RebufferingsNum];

          if (storage[Indicator::State] > state)
            state = storage[Indicator::State];

          bufferLevel.newValue(storage[Indicator::BufferPlayableSize]);
          setupTimer();
        };
        setupTimer();

        boost::asio::deadline_timer runTimer(io);
        runTimer.expires_from_now(boost::posix_time::milliseconds(5000));
        runTimer.wait();
        statTimer.cancel();
        
        EXPECT_NO_THROW(remoteStream.stop());
        EXPECT_NO_THROW(s.stop());
        EXPECT_FALSE(s.isRunning());
    }
    
    publisherFace->shutdown();
    consumerFace->shutdown();
    work.reset();
    t.join();

    GT_PRINTF("Rebufferins: %d, Buffer Level: %.2f\n",
      rebufferingsNum, bufferLevel.value());

    ASSERT_EQ(0, rebufferingsNum);
    EXPECT_GE(state, 5);
    EXPECT_LT(110, bufferLevel.value());
    EXPECT_GT(190, bufferLevel.value());
}
#endif

void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main(int argc, char **argv) {
    signal(SIGABRT, handler);
    signal(SIGSEGV, handler);

    ::testing::InitGoogleTest(&argc, argv);

#ifdef HAVE_BOOST_FILESYSTEM
    boost::filesystem::path testPath(boost::filesystem::system_complete(argv[0]).remove_filename());
    test_path = testPath.string();

    boost::filesystem::path resPath(testPath);
    resPath /= boost::filesystem::path("..") /= 
               boost::filesystem::path("..") /=
               boost::filesystem::path("res");

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
    resources_path = test_path + "../..";
#endif

#ifdef ENABLE_LOGGING
#ifdef HAVE_BOOST_FILESYSTEM
    boost::filesystem::path logsPath(boost::filesystem::path(testPath) /= logs_folder) ;
    if (!boost::filesystem::exists(logsPath) && !boost::filesystem::create_directory(logsPath))
        GT_PRINTF("Failed to create log directory at %s\n", logsPath.string().c_str());
    else
        logs_path = logsPath.string();
#else
    GT_PRINTF("boost::filesystem libarary was not found. Skipped creating folder structure for test logs. "
              "Logs may overwrite each other.\n")
#endif
#endif

    GT_PRINTF("Test path: %s\n", test_path.c_str());
    GT_PRINTF("Resource path: %s\n", resources_path.c_str());
    GT_PRINTF("Test logs path: %s\n", logs_path.c_str());

    return RUN_ALL_TESTS();
}
