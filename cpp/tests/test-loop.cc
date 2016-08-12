//
// test-loop.cc
//
//  Created by Peter Gusev on 17 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

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

#include "gtest/gtest.h"
#include "remote-stream.h"
#include "local-stream.h"
#include "tests-helpers.h"
#include "client/src/video-source.h"
#include "client/src/frame-io.h"
#include "estimators.h"

#include "mock-objects/external-capturer-mock.h"
#include "mock-objects/external-renderer-mock.h"

#define ENABLE_LOGGING
//#define SAVE_VIDEO

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

std::string test_path = "";

MediaStreamParams getSampleVideoParams()
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("low", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 320;
	atp.coderParams_.encodeHeight_ = 240;
	msp.addMediaThread(atp);

  {
    VideoThreadParams atp("mid", sampleVideoCoderParams());
    atp.coderParams_.encodeWidth_ = 320;
    atp.coderParams_.encodeHeight_ = 240;
    msp.addMediaThread(atp);
  }

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
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;
	
	CaptureDeviceParams cdp;
	cdp.deviceId_ = 0;
	msp.captureDevice_ = cdp;
	msp.addMediaThread(AudioThreadParams("hd", "opus"));
	msp.addMediaThread(AudioThreadParams("sd", "g722"));

	return msp;
}

bool checkNfd()
{
  try {
    boost::mutex m;
    boost::unique_lock<boost::mutex> lock(m);
    boost::condition_variable isDone;

    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
    boost::thread t([&isDone, &io](){
        try 
        {
          io.run();
        }
        catch (std::exception& e)
        {
          isDone.notify_one();
        }
    });

    ThreadsafeFace f(io);

    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain("/test");
    f.setCommandSigningInfo(*keyChain, certName(keyName("/test")));

    OnInterestCallback dummy = [](const ptr_lib::shared_ptr<const Name>& prefix,
     const ptr_lib::shared_ptr<const Interest>& interest, Face& face, 
     uint64_t interestFilterId,
     const ptr_lib::shared_ptr<const InterestFilter>& filter){};

    bool registered = false;
    f.registerPrefix(Name("/test"),
      dummy, 
      [&isDone, &registered](const ptr_lib::shared_ptr<const Name>&){
        isDone.notify_one();
      },
      [&isDone, &registered](const ptr_lib::shared_ptr<const Name>&,
        uint64_t registeredPrefixId){
        isDone.notify_one();
        registered = true;
      });

    isDone.wait(lock);

    work.reset();
    io.stop();
    t.join();

    if (!registered)
      throw std::runtime_error("");
  }
  catch (std::exception &e)
  {
    GT_PRINTF("Error creating Face. NFD may not be running. Skipping this test.\n");
    return false;
  }

  return true;
}

TEST(TestLoop, TestVideo)
{
    if (!checkNfd()) return;
    
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
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
    std::string testVideoSource = test_path+"/../../res/test-source-320x240.argb";
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
      // localStream.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
      
      boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
      incomingRawFrame =[&localStream](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
          EXPECT_NO_THROW(localStream.incomingArgbFrame(w, h, data, size));
          return 0;
      };
      
      EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
        .WillRepeatedly(Invoke(incomingRawFrame));
      
      int runTime = 5000;
      
      keyChain->setFace(consumerFace.get());
      RemoteVideoStream rs(io, consumerFace, keyChain, appPrefix, getSampleVideoParams().streamName_);

      queryStat = [&statTimer, &rs, &setupTimer, &rebufferingsNum, &bufferLevel, &state](const boost::system::error_code&){
        StatisticsStorage storage = rs.getStatistics();
        rebufferingsNum = storage[Indicator::RebufferingsNum];
        
        if (storage[Indicator::State] > state)
          state = storage[Indicator::State];

        bufferLevel.newValue(storage[Indicator::BufferPlayableSize]);
        setupTimer();
      };
      setupTimer();
      
#ifdef ENABLE_LOGGING
      rs.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
      
      int waitThreads = 0;
      while (rs.getThreads().size() == 0 && waitThreads++ < 3)
          boost::this_thread::sleep_for(boost::chrono::milliseconds(1500));
      ASSERT_LT(0, rs.getThreads().size());
      
      EXPECT_CALL(renderer, getFrameBuffer(320,240))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(frame->getBuffer().get()));
      
      FileSink frameSink("/tmp/frame-sink.argb");
      boost::function<void(int64_t,int,int,const uint8_t*)> 
        renderFrame = [&nRendered, &frameSink, frame](int64_t,int,int,const uint8_t* buf){
          nRendered++;
          EXPECT_EQ(frame->getBuffer().get(), buf);
#ifdef SAVE_VIDEO
          frameSink << *frame;
#endif
        };
      EXPECT_CALL(renderer, renderBGRAFrame(_,_,_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(renderFrame));

      source.start(30);
      rs.start(rs.getThreads()[0], &renderer);
      boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
      done = true;
      rs.stop();
      source.stop();
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
    EXPECT_GT(190, bufferLevel.value());
}

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
//        s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
        remoteStream.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
        s.start();
        s.isRunning();

        int waitThreads = 0;
        while (remoteStream.getThreads().size() == 0 && waitThreads++ < 3)
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1500));
        ASSERT_LT(0, remoteStream.getThreads().size());
        remoteStream.start(remoteStream.getThreads()[0]);
        
        boost::asio::deadline_timer runTimer(io);
        runTimer.expires_from_now(boost::posix_time::milliseconds(5000));
        runTimer.wait();
        
        EXPECT_NO_THROW(remoteStream.stop());
        EXPECT_NO_THROW(s.stop());
        EXPECT_FALSE(s.isRunning());
    }
    
    publisherFace->shutdown();
    consumerFace->shutdown();
    work.reset();
    t.join();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    test_path = std::string(argv[0]);
    std::vector<std::string> comps;
    boost::split(comps, test_path, boost::is_any_of("/"));
    
    test_path = "";
    for (int i = 0; i < comps.size()-1; ++i) 
    {
        test_path += comps[i];
        if (i != comps.size()-1) test_path += "/";
    }
    
    return RUN_ALL_TESTS();
}
