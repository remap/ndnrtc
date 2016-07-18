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
#include "local-media-stream.h"
#include "tests-helpers.h"
#include "client/src/video-source.h"

#include "mock-objects/external-capturer-mock.h"

#define ENABLE_LOGGING

using namespace ::testing;
using namespace ndnrtc;
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
#if 0
TEST(TestLoop, TestFetchMeta)
{
  if (!checkNfd()) return;

#ifdef ENABLE_LOGGING
  ndnlog::new_api::Logger::initAsyncLogging();
  ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDefault);
#endif

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});
	
  // video source
	boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(320,240));
	std::string testVideoSource = test_path+"/../../res/test-source-320x240.argb";
	VideoSource source(io, testVideoSource, frame);
	MockExternalCapturer capturer;
	source.addCapturer(&capturer);

  boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
  std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
  boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io));
  boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

  publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
  publisherFace->registerPrefix(Name(appPrefix), OnInterestCallback(), 
    [](const boost::shared_ptr<const Name>&){
      ASSERT_FALSE(true);
    });
  
  MediaStreamSettings settings(io, getSampleVideoParams());
  settings.face_ = publisherFace.get();
  settings.keyChain_ = keyChain.get();
  LocalVideoStream localStream(appPrefix, settings);

#ifdef ENABLE_LOGGING
  localStream.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

  boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
    incomingRawFrame =[&localStream](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
      EXPECT_NO_THROW(localStream.incomingArgbFrame(w, h, data, size));
      return 0;
    };

  EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
    .WillRepeatedly(Invoke(incomingRawFrame));

  int runTime = 3000;

  boost::shared_ptr<Face> consumerFace(boost::make_shared<ThreadsafeFace>(io));
  std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app/ndnrtc/%FD%02/video/"+getSampleVideoParams().streamName_;
  keyChain->setFace(consumerFace.get());
  RemoteVideoStream rs(io, consumerFace, keyChain, appPrefix, getSampleVideoParams().streamName_);


#ifdef ENABLE_LOGGING
  rs.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

  source.start(30);
  EXPECT_FALSE(rs.isMetaFetched());
  EXPECT_EQ(0, rs.getThreads().size());
  boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
  source.stop();
  consumerFace->shutdown();
  publisherFace->shutdown();

  EXPECT_TRUE(rs.isMetaFetched());
  EXPECT_EQ(settings.params_.getThreadNum(), rs.getThreads().size());

  for (int i = 0; i < settings.params_.getThreadNum(); ++i)
    EXPECT_EQ(settings.params_.getVideoThread(i)->threadName_, 
      rs.getThreads()[i]);

	work.reset();
	t.join();
}
#endif

#if 0
TEST(TestLoop, TestFetchMetaFailToVerify)
{
  if (!checkNfd()) return;

#ifdef ENABLE_LOGGING
  ndnlog::new_api::Logger::initAsyncLogging();
  ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDefault);
#endif

  boost::asio::io_service io;
  boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
  boost::thread t([&io](){
    io.run();
  });
  
  // video source
  boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(320,240));
  std::string testVideoSource = test_path+"/../../res/test-source-320x240.argb";
  VideoSource source(io, testVideoSource, frame);
  MockExternalCapturer capturer;
  source.addCapturer(&capturer);

  boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
  std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
  boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io));
  boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

  publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
  publisherFace->registerPrefix(Name(appPrefix), OnInterestCallback(), 
    [](const boost::shared_ptr<const Name>&){
      ASSERT_FALSE(true);
    });
  
  MediaStreamSettings settings(io, getSampleVideoParams());
  settings.face_ = publisherFace.get();
  settings.keyChain_ = keyChain.get();
  LocalVideoStream localStream(appPrefix, settings);

#ifdef ENABLE_LOGGING
  localStream.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

  boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
    incomingRawFrame =[&localStream](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
      EXPECT_NO_THROW(localStream.incomingArgbFrame(w, h, data, size));
      return 0;
    };

  EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
    .WillRepeatedly(Invoke(incomingRawFrame));

  int runTime = 3000;

  boost::shared_ptr<Face> consumerFace(boost::make_shared<ThreadsafeFace>(io));
  std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app/ndnrtc/%FD%02/video/"+getSampleVideoParams().streamName_;
  boost::shared_ptr<KeyChain> keyChain2 = memoryKeyChain2(appPrefix);
  keyChain2->setFace(consumerFace.get());
  RemoteVideoStream rs(io, consumerFace, keyChain2, appPrefix, getSampleVideoParams().streamName_);

#ifdef ENABLE_LOGGING
  rs.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

  source.start(30);
  boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
  source.stop();
  consumerFace->shutdown();
  publisherFace->shutdown();
  
  EXPECT_TRUE(rs.isMetaFetched());
  EXPECT_EQ(2, rs.getThreads().size());
  EXPECT_FALSE(rs.isVerified());

  work.reset();
  io.stop();
  t.join();
}
#endif

#if 1
TEST(TestLoop, TestFetch30Sec)
{
    if (!checkNfd()) return;
    
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
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
    source.addCapturer(&capturer);
    
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    publisherFace->registerPrefix(Name(appPrefix), OnInterestCallback(),
                                  [](const boost::shared_ptr<const Name>&){
                                      ASSERT_FALSE(true);
                                  });
    
    MediaStreamSettings settings(io, getSampleVideoParams());
    settings.face_ = publisherFace.get();
    settings.keyChain_ = keyChain.get();
    LocalVideoStream localStream(appPrefix, settings);
    
#ifdef ENABLE_LOGGING
     localStream.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
    
    boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
    incomingRawFrame =[&localStream](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
        EXPECT_NO_THROW(localStream.incomingArgbFrame(w, h, data, size));
        return 0;
    };
    
    EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
    .WillRepeatedly(Invoke(incomingRawFrame));
    
    int runTime = 5000;
    
    boost::shared_ptr<Face> consumerFace(boost::make_shared<ThreadsafeFace>(io));
    keyChain->setFace(consumerFace.get());
    RemoteVideoStream rs(io, consumerFace, keyChain, appPrefix, getSampleVideoParams().streamName_);
    
#ifdef ENABLE_LOGGING
//    rs.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
    
    int waitThreads = 0;
    while (rs.getThreads().size() == 0 && waitThreads++ < 3)
        boost::this_thread::sleep_for(boost::chrono::milliseconds(1500));
    ASSERT_LT(0, rs.getThreads().size());
    
    source.start(30);
    rs.start(rs.getThreads()[0]);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
    rs.stop();
    source.stop();
    consumerFace->shutdown();
    publisherFace->shutdown();
    
    work_source.reset();
    io_source.stop();
    t_source.join();
    
    work.reset();
    io.stop();
    t.join();
}
#endif


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
