//
// test-persistent-storage.cc
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#ifndef __ANDROID__
#include <execinfo.h>
#endif

#define ENABLE_LOGGING

#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <boost/assign.hpp>
#include <boost/asio.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
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

#include "frame-converter.hpp"
#include "gtest/gtest.h"
#include "tests-helpers.hpp"
#include "remote-stream.hpp"
#include "local-stream.hpp"
#include "client/src/video-source.hpp"
#include "client/src/frame-io.hpp"
#include "src/packet-publisher.hpp"
#include "mock-objects/ndn-cpp-mock.hpp"
#include "frame-data.hpp"
#include "video-thread.hpp"
#include "interfaces.hpp"

#include "persistent-storage/fetching-task.hpp"
#include "storage-engine.hpp"
#include "frame-fetcher.hpp"
#include "frame-buffer.hpp"
#include "local-stream.hpp"

#include "mock-objects/external-capturer-mock.hpp"

// include .cpp in order to instantiate class with mock objects
#include "src/packet-publisher.cpp"

#ifdef __ANDROID__
#define LEVELDB
#else
// #define HYPERDB
#define ROCKSDB
#endif

#include <cassert>
#ifdef LEVELDB
    #include <leveldb/db.h>
    namespace db_namespace = leveldb;
#endif
#ifdef HYPERDB
    #include <hyperleveldb/db.h>
    namespace db_namespace = leveldb;
#endif
#ifdef ROCKSDB
    #include <rocksdb/db.h>
    namespace db_namespace = rocksdb;
#endif

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

typedef _PublisherSettings<MockNdnKeyChain, MockNdnMemoryCache> MockSettings;
typedef std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest>> PendingInterests;

#if 0
TEST(TestPeristentStorage, TestBenchmarkSegment8000)
{

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

    db_namespace::DB* db;
    db_namespace::Options options;
    options.create_if_missing = true;
    db_namespace::Status status = db_namespace::DB::Open(options, dbPath, &db);
    assert(status.ok());

    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

    PublisherSettings settings;

    uint64_t dbInsertOps = 0;
    uint64_t dbInsertUsec = 0;
    int wireLength = 8000;
    int freshness = 1000;
    settings.sign_ = false;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
    settings.onSegmentsCached_ = [db, &dbInsertUsec, &dbInsertOps](std::vector<boost::shared_ptr<const ndn::Data>> segments){
        for (auto d:segments)
        {
            dbInsertOps++;
            // put
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            db->Put(db_namespace::WriteOptions(), d->getName().toUri(), 
                    db_namespace::Slice((const char*)d->wireEncode().buf(), d->wireEncode().size()));
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbInsertUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
        }
    };

    int nFrames = 1000;
    int frameNo = 0;
    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d");

    VideoPacketPublisher publisher(settings);

#ifdef ENABLE_LOGGING
    publisher.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    for (int frameLen = 10000; frameLen < 35000; frameLen += 10000)
    {
        unsigned int publishDuration = 0;
        unsigned int totalSlices = 0;

        for (int i = 0; i < nFrames; ++i)
        {
            Name packetName(filter);
            packetName.appendSequenceNumber(frameNo++);

            CommonHeader hdr;
            hdr.sampleRate_ = 24.7;
            hdr.publishTimestampMs_ = 488589553;
            hdr.publishUnixTimestamp_ = 1460488589;

            int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
            uint8_t *buffer = (uint8_t *)malloc(frameLen);
            for (int i = 0; i < frameLen; ++i)
                buffer[i] = i % 255;

            webrtc::EncodedImage frame(buffer, frameLen, size);
            frame._encodedWidth = 640;
            frame._encodedHeight = 480;
            frame._timeStamp = 1460488589;
            frame.capture_time_ms_ = 1460488569;
            frame._frameType = webrtc::kVideoFrameKey;
            frame._completeFrame = true;

            VideoFramePacket vp(frame);
            std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

            vp.setSyncList(syncList);
            vp.setHeader(hdr);

            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2 - t1).count();
            totalSlices += segments.size();
        }

        GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f segments per frame average). Average frame publishing time is %.2fms\n",
                  nFrames, frameLen, (double)totalSlices / (double)nFrames, (double)publishDuration / (double)nFrames);
    }

    GT_PRINTF("DB insert ops %d. Average insertion time %2.fusec\n",
              dbInsertOps, (double)dbInsertUsec/(double)dbInsertOps);

    uint64_t dbReadUsec = 0, dbGoodReadUsec = 0, dbBadReadUsec = 0;
    unsigned int dbReadOps = 0;
    unsigned int dbGoodReads = 0, dbBadReads = 0;

    // extract from db
    for (int i = 0; i < frameNo; ++i)
    {
        Name packetName(filter);
        packetName.appendSequenceNumber(i);

        for (int k = 0; k < 5; ++k)
        {
            Name segmentName(packetName);
            segmentName.appendSegment(k);

            std::string blob;
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            db_namespace::Status s = db->Get(db_namespace::ReadOptions(), segmentName.toUri(), &blob);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            dbReadOps++;

            if (s.ok())
            {
                dbGoodReads++;
                dbGoodReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            }
            else
            {
                dbBadReads++;
                dbBadReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            }
        }
    }

    GT_PRINTF("DB read ops %d (%d good, %d bad). Average read time %.2fusec, good %.2fusec, bad %.2fusec\n",
              dbReadOps, dbGoodReads, dbBadReads, (double)dbReadUsec/(double)dbReadOps,
              (double)dbGoodReadUsec/(double)dbGoodReads, (double)dbBadReadUsec/(double)dbBadReads);

    delete db;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

#if 0
TEST(TestPersistentStorage, TestWriteMillion)
{
#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

    db_namespace::DB* db;
    db_namespace::Options options;
    options.create_if_missing = true;
    db_namespace::Status status = db_namespace::DB::Open(options, dbPath, &db);
    assert(status.ok());


    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

    uint64_t dbInsertOps = 0;
    uint64_t dbInsertUsec = 0;
    int wireLength = 8000;
    int freshness = 1000;

    unsigned char *packet = (unsigned char*)malloc(wireLength);
    int nPackets = 1000000;
    int frameNo = 0, segNo = 0;
    int batch = 0, batchSize = 1000;
    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d");
    boost::chrono::high_resolution_clock::time_point t1, t2;

    for (int i = 0; i < nPackets; ++i)
    {
        Name packetName(filter);

        batch++;
        frameNo = i/30;
        segNo = i%30;
        packetName.appendSequenceNumber(frameNo).appendSegment(segNo);

        // if (batch == 0)
            
        t1 = boost::chrono::high_resolution_clock::now();
        db->Put(db_namespace::WriteOptions(), packetName.toUri(),
                db_namespace::Slice((const char *)packet, wireLength));
        t2 = boost::chrono::high_resolution_clock::now();
        dbInsertUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();

        if (batch == batchSize)
        {
            int64_t timeUsec = dbInsertUsec;
            int64_t insertTime = (int64_t)((double)timeUsec/(double)batchSize);
            double throughput = (double)batchSize / ((double)timeUsec/1000000.);

            dbInsertUsec = 0;

            std::cout << i << "\t" << insertTime << "\t" << throughput << std::endl;

            batch = 0;
        }
    }

    std::string keysNum;
    bool res = db->GetProperty("rocksdb.estimate-num-keys", &keysNum);
    if (!res) keysNum = "N/A";

    namespace bf=boost::filesystem;
    int64_t dbSize=0;
    for(bf::recursive_directory_iterator it(dbPath); it!=bf::recursive_directory_iterator(); ++it)
    {   
        if(!bf::is_directory(*it))
            dbSize += bf::file_size(*it);
    } 
     
    GT_PRINTF("db size: %d bytes (%.0f Mbytes), est Keys num %s\n", 
              dbSize, ((double)dbSize)/1024./1024., keysNum.c_str());

    // uint64_t dbReadUsec = 0, dbGoodReadUsec = 0, dbBadReadUsec = 0;
    // unsigned int dbReadOps = 0;
    // unsigned int dbGoodReads = 0, dbBadReads = 0;

    // extract from db
    // for (int i = 0; i < frameNo; ++i)
    // {
    //     Name packetName(filter);
    //     packetName.appendSequenceNumber(i);

    //     for (int k = 0; k < 5; ++k)
    //     {
    //         Name segmentName(packetName);
    //         segmentName.appendSegment(k);

    //         std::string blob;
    //         boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
    //         db_namespace::Status s = db->Get(db_namespace::ReadOptions(), segmentName.toUri(), &blob);
    //         boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
    //         dbReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
    //         dbReadOps++;

    //         if (s.ok())
    //         {
    //             dbGoodReads++;
    //             dbGoodReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
    //         }
    //         else
    //         {
    //             dbBadReads++;
    //             dbBadReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
    //         }
    //     }
    // }

    // GT_PRINTF("DB read ops %d (%d good, %d bad). Average read time %.2fusec, good %.2fusec, bad %.2fusec\n",
    //           dbReadOps, dbGoodReads, dbBadReads, (double)dbReadUsec/(double)dbReadOps,
    //           (double)dbGoodReadUsec/(double)dbGoodReads, (double)dbBadReadUsec/(double)dbBadReads);

    free(packet);
    delete db;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

std::string resources_path = "";
std::string test_path = "";
#if 0
TEST(TestPersistentStorage, TestBenchmarkVideoPublishing)
{
#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

    db_namespace::DB* db;
    db_namespace::Options options;
    options.create_if_missing = true;
    db_namespace::Status status = db_namespace::DB::Open(options, dbPath, &db);
    assert(status.ok());

    boost::asio::io_service io_source;
    boost::shared_ptr<boost::asio::io_service::work> work_source(boost::make_shared<boost::asio::io_service::work>(io_source));
    boost::thread t_source([&io_source](){
        io_source.run();
    });

    int runTime = 5*60*1000;
    // int width = 1280, height = 720, bitrate = 3000;
    int width = 320, height = 240, bitrate = 1000;
    boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(width,height));
    // std::string testVideoSource = resources_path+"/test-source-1280x720.argb";
    std::string testVideoSource = resources_path+"/test-source-320x240.argb";
    VideoSource source(io_source, testVideoSource, frame);
    MockExternalCapturer capturer;
    source.addCapturer(&capturer);

    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io_source));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(publisherFace.get());

    PublisherSettings settings;

    uint64_t dbInsertOps = 0;
    uint64_t dbInsertUsec = 0;
    uint64_t dbKeyLength = 0;
    uint64_t dbValueLength = 0;
    int wireLength = 8000;
    int freshness = 1000;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
    settings.onSegmentsCached_ = [db, &dbInsertUsec, &dbInsertOps, &dbKeyLength, &dbValueLength](std::vector<boost::shared_ptr<const ndn::Data>> segments){
        for (auto d:segments)
        {
            dbInsertOps++;
            dbKeyLength += d->getName().toUri().size();
            dbValueLength += d->wireEncode().size();

            // std::cout << "will put" << d->getName() << std::endl;

            // put
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            db->Put(db_namespace::WriteOptions(), d->getName().toUri(), 
                    db_namespace::Slice((const char*)d->wireEncode().buf(), d->wireEncode().size()));
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbInsertUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
        }
    };

    VideoPacketPublisher publisher(settings);
    RawFrameConverter conv;

    VideoCoderParams vcp(sampleVideoCoderParams());
    vcp.startBitrate_ = bitrate;
    vcp.maxBitrate_ = bitrate;
    vcp.encodeWidth_ = width;
    vcp.encodeHeight_ = height;
    VideoThread vt(vcp);

    std::string streamPrefix = "/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6";
    std::string thread = "tiny";
    PacketNumber keyNo = 0, deltaNo = 0, playNo = 0;
    int maxKeySegNum  = 0, maxDeltaSegNum = 0;

    boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
      incomingRawFrame =[&publisher, &vt, &conv, wireLength, &keyNo, &deltaNo, &playNo, streamPrefix, thread,
                         &maxKeySegNum, &maxDeltaSegNum]
                        (const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size)
    {
        // encode a frame and pass it to the publisher

        boost::shared_ptr<VideoFramePacket> vf(boost::move(vt.encode((conv << ArgbRawFrameWrapper({w, h, data, size})))));

        if (!vf.get())
            return 0;

        bool isKey = (vf->getFrame()._frameType == webrtc::kVideoFrameKey);
        CommonHeader packetHdr;
        packetHdr.sampleRate_ = 30;
        packetHdr.publishTimestampMs_ = 1234566789;
        packetHdr.publishUnixTimestamp_ = 1234566789;
        vf->setHeader(packetHdr);

        boost::shared_ptr<NetworkData> parityData = vf->getParityData(VideoFrameSegment::payloadLength(wireLength), 0.2);
        PacketNumber seqNo = (isKey ? keyNo : deltaNo);
        PacketNumber pairedSeq = (isKey ? deltaNo + 1 : keyNo);
        PacketNumber playbackNo = playNo;

        Name dataName(streamPrefix);
        dataName.append(thread)
        .append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
        .appendSequenceNumber(seqNo);
        size_t nDataSeg = VideoFrameSegment::numSlices(*vf, wireLength);
        size_t nParitySeg = VideoFrameSegment::numSlices(*parityData, wireLength);

        VideoFrameSegmentHeader segmentHdr;
        segmentHdr.totalSegmentsNum_ = nDataSeg;
        segmentHdr.paritySegmentsNum_ = nParitySeg;
        segmentHdr.playbackNo_ = playbackNo;
        segmentHdr.pairedSequenceNo_ = pairedSeq;

        PublishedDataPtrVector segments =
            publisher.publish(dataName, *vf, segmentHdr, (isKey ? 900 : -1), isKey, true);
        assert(segments.size());

        if (isKey)
        {
            if (segments.size() > maxKeySegNum)
                maxKeySegNum = segments.size();
        }
        else
        {
            if (segments.size() > maxDeltaSegNum)
                maxDeltaSegNum = segments.size();
        }

        PublishedDataPtrVector paritySegments;
        if (nParitySeg)
        {
            Name parityName(dataName);
            parityName.append(NameComponents::NameComponentParity);

            paritySegments = publisher.publish(parityName, *parityData, segmentHdr, (isKey ? 900: -1), isKey);
        }

        if (isKey)
            keyNo ++;
        else
            deltaNo ++;

        return 0;
    };

    EXPECT_CALL(capturer, incomingArgbFrame(width, height, _, _))
        .WillRepeatedly(Invoke(incomingRawFrame));

    source.start(30);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
    work_source.reset();
    io_source.stop();
    t_source.join();

    GT_PRINTF("video %dx%d, %d sec, bitrate %dkbit/sec, published frames: %d key, %d delta, (max segnum %d %d)\n", 
            width, height, runTime/1000, bitrate, keyNo, deltaNo, maxKeySegNum, maxDeltaSegNum);
    GT_PRINTF("insert ops %d, avg insertion time %2.fusec\n",
              dbInsertOps, (double)dbInsertUsec/(double)dbInsertOps);
    GT_PRINTF("avg key len %.2f bytes, avg blob len %.2f bytes\n",
              (double)dbKeyLength/(double)dbInsertOps, (double)dbValueLength/(double)dbInsertOps);

    namespace bf=boost::filesystem;
    size_t dbSize=0;
    for(bf::recursive_directory_iterator it(dbPath); it!=bf::recursive_directory_iterator(); ++it)
    {   
        if(!bf::is_directory(*it))
            dbSize += bf::file_size(*it);
    }  
    GT_PRINTF("db size: %d bytes (%.0f Mbytes)\n", dbSize, ((double)dbSize)/1024./1024.);

    uint64_t dbReadUsec = 0, dbGoodReadUsec = 0, dbBadReadUsec = 0;
    unsigned int dbReadOps = 0;
    unsigned int dbGoodReads = 0, dbBadReads = 0;

    // extract from db
    bool isKey = false;
    int k = 0, d = 0;
    for (int i = 0; k < keyNo && d < deltaNo; ++i)
    {
        bool prevIsKey = isKey;
        isKey = (d % 30 == 0) && !prevIsKey;
        PacketNumber seqNo = (isKey ? k : d);
        Name dataName(streamPrefix);

        dataName.append(thread)
        .append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
        .appendSequenceNumber(seqNo);   

        int nSegments = (isKey ? maxKeySegNum : maxDeltaSegNum);

        for (int seg = 0; seg < nSegments; ++seg)
        {
            Name segmentName(dataName);
            segmentName.appendSegment(seg);

            std::string blob;
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            db_namespace::Status s = db->Get(db_namespace::ReadOptions(), segmentName.toUri(), &blob);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            dbReadOps++;

            if (s.ok())
            {
                dbGoodReads++;
                dbGoodReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            }
            else
            {
                dbBadReads++;
                dbBadReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            }
        }

        if (isKey) k++;
        else d++;
    }

    GT_PRINTF("DB read ops %d (%d good, %d bad). Average read time %.2fusec, good %.2fusec, bad %.2fusec\n",
              dbReadOps, dbGoodReads, dbBadReads, (double)dbReadUsec/(double)dbReadOps,
              (double)dbGoodReadUsec/(double)dbGoodReads, (double)dbBadReadUsec/(double)dbBadReads);

    delete db;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

#if 0
TEST(TestPeristentStorage, TestStorageEngine)
{

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

    boost::shared_ptr<StorageEngine> storage = boost::make_shared<StorageEngine>(dbPath);

    Face face("aleph.ndn.ucla.edu");
    boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

    PublisherSettings settings;
    std::set<std::string> insertedData;

    uint64_t dbInsertOps = 0;
    uint64_t dbInsertUsec = 0;
    int wireLength = 8000;
    int freshness = 1000;
    settings.sign_ = false;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
    settings.onSegmentsCached_ = [storage, &insertedData, &dbInsertUsec, &dbInsertOps](std::vector<boost::shared_ptr<const ndn::Data>> segments){
        for (auto d:segments)
        {
            insertedData.insert(d->getName().toUri());
            dbInsertOps++;
            // put
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            storage->put(d);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbInsertUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
        }
    };

    int nFrames = 10;
    int frameNo = 0;
    Name filter("/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%02/video/camera/%FC%00%00%01c_%27%DE%D6/tiny/d");

    VideoPacketPublisher publisher(settings);

#ifdef ENABLE_LOGGING
    publisher.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    int frameLen = 10000;
    // for (int frameLen = 10000; frameLen < 35000; frameLen += 10000)
    {
        unsigned int publishDuration = 0;
        unsigned int totalSlices = 0;

        for (int i = 0; i < nFrames; ++i)
        {
            Name packetName(filter);
            packetName.appendSequenceNumber(frameNo++);

            CommonHeader hdr;
            hdr.sampleRate_ = 24.7;
            hdr.publishTimestampMs_ = 488589553;
            hdr.publishUnixTimestamp_ = 1460488589;

            int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
            uint8_t *buffer = (uint8_t *)malloc(frameLen);
            for (int i = 0; i < frameLen; ++i)
                buffer[i] = i % 255;

            webrtc::EncodedImage frame(buffer, frameLen, size);
            frame._encodedWidth = 640;
            frame._encodedHeight = 480;
            frame._timeStamp = 1460488589;
            frame.capture_time_ms_ = 1460488569;
            frame._frameType = webrtc::kVideoFrameKey;
            frame._completeFrame = true;

            VideoFramePacket vp(frame);
            std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

            vp.setSyncList(syncList);
            vp.setHeader(hdr);

            boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
            VideoFrameSegmentHeader segHdr;
            segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
            segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
            segHdr.playbackNo_ = 100;
            segHdr.pairedSequenceNo_ = 67;

            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            PublishedDataPtrVector segments = publisher.publish(packetName, vp, segHdr, freshness);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2 - t1).count();
            totalSlices += segments.size();

            free(buffer);
        }

        GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f segments per frame average). Average frame publishing time is %.2fms\n",
                  nFrames, frameLen, (double)totalSlices / (double)nFrames, (double)publishDuration / (double)nFrames);
    }

    GT_PRINTF("DB insert ops %d. Average insertion time %2.fusec\n",
              dbInsertOps, (double)dbInsertUsec/(double)dbInsertOps);

    uint64_t dbReadUsec = 0, dbGoodReadUsec = 0, dbBadReadUsec = 0;
    unsigned int dbReadOps = 0;
    unsigned int dbGoodReads = 0, dbBadReads = 0;

    EXPECT_GT(insertedData.size(), 0);
    // extract from db
    for (int i = 0; i < frameNo; ++i)
    {
        Name packetName(filter);
        packetName.appendSequenceNumber(i);

        for (int k = 0; k < 5; ++k)
        {
            Name segmentName(packetName);
            segmentName.appendSegment(k);

            std::string blob;
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            boost::shared_ptr<ndn::Data> data = storage->get(segmentName);
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            dbReadOps++;

            if (data.get())
            {
                insertedData.erase(data->getName().toUri());
                dbGoodReads++;
                dbGoodReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            }
            else
            {
                dbBadReads++;
                dbBadReadUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
            }
        }
    }

    GT_PRINTF("DB read ops %d (%d good, %d bad). Average read time %.2fusec, good %.2fusec, bad %.2fusec\n",
              dbReadOps, dbGoodReads, dbBadReads, (double)dbReadUsec/(double)dbReadOps,
              (double)dbGoodReadUsec/(double)dbGoodReads, (double)dbBadReadUsec/(double)dbBadReads);
    EXPECT_EQ(0, insertedData.size());

    storage.reset();

    db_namespace::Options options;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

#if 0
TEST(TestPersistentStorage, TestFrameFetchingTask)
{
#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    boost::shared_ptr<StorageEngine> storage = boost::make_shared<StorageEngine>(dbPath);

    boost::asio::io_service io_source;
    boost::shared_ptr<boost::asio::io_service::work> work_source(boost::make_shared<boost::asio::io_service::work>(io_source));
    boost::thread t_source([&io_source](){
        io_source.run();
    });

    int runTime = 1*5*1000;
    // int width = 1280, height = 720, bitrate = 3000;
    int width = 320, height = 240, bitrate = 1000;
    boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(width,height));
    // std::string testVideoSource = resources_path+"/test-source-1280x720.argb";
    std::string testVideoSource = resources_path+"/test-source-320x240.argb";
    VideoSource source(io_source, testVideoSource, frame);
    MockExternalCapturer capturer;
    source.addCapturer(&capturer);

    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io_source));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(publisherFace.get());

    PublisherSettings settings;

    uint64_t dbInsertOps = 0;
    uint64_t dbInsertUsec = 0;
    uint64_t dbKeyLength = 0;
    uint64_t dbValueLength = 0;
    int wireLength = 8000;
    int freshness = 1000;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
    settings.onSegmentsCached_ = [storage, &dbInsertUsec, &dbInsertOps, &dbKeyLength, &dbValueLength](std::vector<boost::shared_ptr<const ndn::Data>> segments){
        for (auto d:segments)
        {
            dbInsertOps++;
            dbKeyLength += d->getName().toUri().size();
            dbValueLength += d->wireEncode().size();

            // std::cout << "will put" << d->getName() << std::endl;

            // put
            boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
            storage->put(d);
            // db->Put(db_namespace::WriteOptions(), d->getName().toUri(), 
            //         db_namespace::Slice((const char*)d->wireEncode().buf(), d->wireEncode().size()));
            boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
            dbInsertUsec += boost::chrono::duration_cast<boost::chrono::microseconds>(t2 - t1).count();
        }
    };

    VideoPacketPublisher publisher(settings);
    RawFrameConverter conv;

    VideoCoderParams vcp(sampleVideoCoderParams());
    vcp.startBitrate_ = bitrate;
    vcp.maxBitrate_ = bitrate;
    vcp.encodeWidth_ = width;
    vcp.encodeHeight_ = height;
    VideoThread vt(vcp);

    std::string streamPrefix = "/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6";
    std::string thread = "tiny";
    PacketNumber keyNo = 0, deltaNo = 0, playNo = 0;
    int maxKeySegNum  = 0, maxDeltaSegNum = 0;

    boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
      incomingRawFrame =[&publisher, &vt, &conv, wireLength, &keyNo, &deltaNo, &playNo, streamPrefix, thread,
                         &maxKeySegNum, &maxDeltaSegNum]
                        (const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size)
    {
        // encode a frame and pass it to the publisher

        boost::shared_ptr<VideoFramePacket> vf(boost::move(vt.encode((conv << ArgbRawFrameWrapper({w, h, data, size})))));

        if (!vf.get())
            return 0;

        bool isKey = (vf->getFrame()._frameType == webrtc::kVideoFrameKey);
        CommonHeader packetHdr;
        packetHdr.sampleRate_ = 30;
        packetHdr.publishTimestampMs_ = 1234566789;
        packetHdr.publishUnixTimestamp_ = 1234566789;
        vf->setHeader(packetHdr);

        boost::shared_ptr<NetworkData> parityData = vf->getParityData(VideoFrameSegment::payloadLength(wireLength), 0.2);
        PacketNumber seqNo = (isKey ? keyNo : deltaNo);
        PacketNumber pairedSeq = (isKey ? deltaNo + 1 : keyNo);
        PacketNumber playbackNo = playNo;

        Name dataName(streamPrefix);
        dataName.append(thread)
        .append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
        .appendSequenceNumber(seqNo);
        size_t nDataSeg = VideoFrameSegment::numSlices(*vf, wireLength);
        size_t nParitySeg = VideoFrameSegment::numSlices(*parityData, wireLength);

        VideoFrameSegmentHeader segmentHdr;
        segmentHdr.totalSegmentsNum_ = nDataSeg;
        segmentHdr.paritySegmentsNum_ = nParitySeg;
        segmentHdr.playbackNo_ = playbackNo;
        segmentHdr.pairedSequenceNo_ = pairedSeq;

        PublishedDataPtrVector segments =
            publisher.publish(dataName, *vf, segmentHdr, (isKey ? 900 : -1), isKey, true);
        assert(segments.size());

        if (isKey)
        {
            if (segments.size() > maxKeySegNum)
                maxKeySegNum = segments.size();
        }
        else
        {
            if (segments.size() > maxDeltaSegNum)
                maxDeltaSegNum = segments.size();
        }

        PublishedDataPtrVector paritySegments;
        if (nParitySeg)
        {
            Name parityName(dataName);
            parityName.append(NameComponents::NameComponentParity);

            paritySegments = publisher.publish(parityName, *parityData, segmentHdr, (isKey ? 900: -1), isKey);
        }

        if (isKey)
            keyNo ++;
        else
            deltaNo ++;

        return 0;
    };

    EXPECT_CALL(capturer, incomingArgbFrame(width, height, _, _))
        .WillRepeatedly(Invoke(incomingRawFrame));

    source.start(30);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
    work_source.reset();
    io_source.stop();
    t_source.join();

    GT_PRINTF("video %dx%d, %d sec, bitrate %dkbit/sec, published frames: %d key, %d delta, (max segnum %d %d)\n", 
            width, height, runTime/1000, bitrate, keyNo, deltaNo, maxKeySegNum, maxDeltaSegNum);

   
    uint64_t dbReadUsec = 0, dbGoodReadUsec = 0, dbBadReadUsec = 0;
    unsigned int dbReadOps = 0;
    unsigned int dbGoodReads = 0, dbBadReads = 0;

    // extract from db
    std::vector<boost::shared_ptr<FrameFetchingTask>> fetchingTasks;
    boost::shared_ptr<FetchMethodLocal> localFetch = boost::make_shared<FetchMethodLocal>(storage);

    int nFetchedFrames = 0, nZeroSegmentCalls = 0, nFirstSegmentCalls = 0;
    OnFetchingComplete onFrameFetched = 
        [&nFetchedFrames](const boost::shared_ptr<const FetchingTask>& task, 
                          const boost::shared_ptr<const BufferSlot>& slot)
        {
            nFetchedFrames++;
            EXPECT_EQ(FrameFetchingTask::Completed, 
                      boost::dynamic_pointer_cast<const FrameFetchingTask>(task)->getState());
        };
    OnFetchingFailed onFrameFetchingFailed = 
        [](const boost::shared_ptr<const FetchingTask>& taks,
           std::string reason){
            FAIL() << "Failed to fetch frame: something is seriously wrong";
        };
    OnSegment onFirstSegment = 
        [&nFirstSegmentCalls](const boost::shared_ptr<const FetchingTask>& task,
                              const boost::shared_ptr<const SlotSegment>& segment)
        {
            EXPECT_EQ(FrameFetchingTask::Fetching, 
                      boost::dynamic_pointer_cast<const FrameFetchingTask>(task)->getState());
        };
    OnSegment onZeroSegment = 
        [&nZeroSegmentCalls](const boost::shared_ptr<const FetchingTask>& task,
                             const boost::shared_ptr<const SlotSegment>& segment)
        {
            EXPECT_EQ(FrameFetchingTask::Fetching, 
                      boost::dynamic_pointer_cast<const FrameFetchingTask>(task)->getState());
        };

    bool isKey = false;
    int k = 0, d = 0;
    for (int i = 0; k < keyNo && d < deltaNo; ++i)
    {
        bool prevIsKey = isKey;
        isKey = (d % 30 == 0) && !prevIsKey;
        PacketNumber seqNo = (isKey ? k : d);
        Name dataName(streamPrefix);

        dataName.append(thread)
        .append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
        .appendSequenceNumber(seqNo);   

        boost::shared_ptr<FrameFetchingTask> fetchingTask = 
            boost::make_shared<FrameFetchingTask>(dataName, localFetch,
                                                  onFrameFetched, onFrameFetchingFailed,
                                                  FrameFetchingTask::Settings({3, 1000}),
                                                  onFirstSegment, onZeroSegment);
        fetchingTask->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
        fetchingTask->start();
        fetchingTasks.push_back(fetchingTask);

        if (isKey) k++;
        else d++;
    }
    
    // EXPECT_EQ(nFetchedFrames, keyNo+deltaNo);
    for (auto task:fetchingTasks)
        EXPECT_EQ(FrameFetchingTask::Completed, task->getState());

    storage.reset();

    db_namespace::Options options;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

#if 0
TEST(TestPersistentStorage, TestFrameFetcher)
{
#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

    boost::shared_ptr<StorageEngine> storage = boost::make_shared<StorageEngine>(dbPath);

    boost::asio::io_service io_source;
    boost::shared_ptr<boost::asio::io_service::work> work_source(boost::make_shared<boost::asio::io_service::work>(io_source));
    boost::thread t_source([&io_source](){
        io_source.run();
    });

    int runTime = 1*5*1000;
    // int width = 1280, height = 720, bitrate = 3000;
    int width = 320, height = 240, bitrate = 1000;
    boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(width,height));
    // std::string testVideoSource = resources_path+"/test-source-1280x720.argb";
    std::string testVideoSource = resources_path+"/test-source-320x240.argb";
    VideoSource source(io_source, testVideoSource, frame);
    MockExternalCapturer capturer;
    source.addCapturer(&capturer);

    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io_source));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(publisherFace.get());

    PublisherSettings settings;

    int wireLength = 8000;
    int freshness = 1000;
    settings.keyChain_ = keyChain.get();
    settings.memoryCache_ = memCache.get();
    settings.segmentWireLength_ = wireLength;
    settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
    settings.onSegmentsCached_ = [storage](std::vector<boost::shared_ptr<const ndn::Data>> segments){
        for (auto d:segments)
            storage->put(d);
    };

    VideoPacketPublisher publisher(settings);
    RawFrameConverter conv;

    VideoCoderParams vcp(sampleVideoCoderParams());
    vcp.startBitrate_ = bitrate;
    vcp.maxBitrate_ = bitrate;
    vcp.encodeWidth_ = width;
    vcp.encodeHeight_ = height;
    VideoThread vt(vcp);

    std::string streamPrefix = "/ndn/edu/wustl/jdd/clientA/ndnrtc/%FD%03/video/camera/%FC%00%00%01c_%27%DE%D6";
    std::string thread = "tiny";
    PacketNumber keyNo = 0, deltaNo = 0, playNo = 0;
    int maxKeySegNum  = 0, maxDeltaSegNum = 0;

    boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
      incomingRawFrame =[&publisher, &vt, &conv, wireLength, &keyNo, &deltaNo, &playNo, streamPrefix, thread,
                         &maxKeySegNum, &maxDeltaSegNum]
                        (const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size)
    {
        // encode a frame and pass it to the publisher

        boost::shared_ptr<VideoFramePacket> vf(boost::move(vt.encode((conv << ArgbRawFrameWrapper({w, h, data, size})))));

        if (!vf.get())
            return 0;

        bool isKey = (vf->getFrame()._frameType == webrtc::kVideoFrameKey);
        CommonHeader packetHdr;
        packetHdr.sampleRate_ = 30;
        packetHdr.publishTimestampMs_ = 1234566789;
        packetHdr.publishUnixTimestamp_ = 1234566789;
        vf->setHeader(packetHdr);

        boost::shared_ptr<NetworkData> parityData = vf->getParityData(VideoFrameSegment::payloadLength(wireLength), 0.2);
        PacketNumber seqNo = (isKey ? keyNo : deltaNo);
        PacketNumber pairedSeq = (isKey ? deltaNo : keyNo-1);
        PacketNumber playbackNo = playNo;

        Name dataName(streamPrefix);
        dataName.append(thread)
        .append((isKey ? NameComponents::NameComponentKey : NameComponents::NameComponentDelta))
        .appendSequenceNumber(seqNo);
        size_t nDataSeg = VideoFrameSegment::numSlices(*vf, wireLength);
        size_t nParitySeg = VideoFrameSegment::numSlices(*parityData, wireLength);

        VideoFrameSegmentHeader segmentHdr;
        segmentHdr.totalSegmentsNum_ = nDataSeg;
        segmentHdr.paritySegmentsNum_ = nParitySeg;
        segmentHdr.playbackNo_ = playbackNo;
        segmentHdr.pairedSequenceNo_ = pairedSeq;

        PublishedDataPtrVector segments =
            publisher.publish(dataName, *vf, segmentHdr, (isKey ? 900 : -1), isKey, true);
        assert(segments.size());

        if (isKey)
        {
            if (segments.size() > maxKeySegNum)
                maxKeySegNum = segments.size();
        }
        else
        {
            if (segments.size() > maxDeltaSegNum)
                maxDeltaSegNum = segments.size();
        }

        PublishedDataPtrVector paritySegments;
        if (nParitySeg)
        {
            Name parityName(dataName);
            parityName.append(NameComponents::NameComponentParity);

            paritySegments = publisher.publish(parityName, *parityData, segmentHdr, (isKey ? 900: -1), isKey);
        }

        if (isKey)
            keyNo ++;
        else
            deltaNo ++;

        return 0;
    };

    EXPECT_CALL(capturer, incomingArgbFrame(width, height, _, _))
        .WillRepeatedly(Invoke(incomingRawFrame));

    source.start(30);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
    work_source.reset();
    io_source.stop();
    t_source.join();

    GT_PRINTF("video %dx%d, %d sec, bitrate %dkbit/sec, published frames: %d key, %d delta, (max segnum %d %d)\n", 
            width, height, runTime/1000, bitrate, keyNo, deltaNo, maxKeySegNum, maxDeltaSegNum);


    // extract from db
    boost::shared_ptr<FrameFetcher> fetcher = boost::make_shared<FrameFetcher>(storage);
    uint8_t* frameBuffer = nullptr;
    int nFetched = 0;
    Name fetchedFrameName;
    boost::chrono::high_resolution_clock::time_point fetchingSpawned;
    OnBufferAllocate onBufferAllocate = 
        [&frameBuffer](const boost::shared_ptr<IFrameFetcher>& fetcher, int width, int height)->uint8_t*
        {
            frameBuffer = (uint8_t*)malloc(width*height*4);
            return frameBuffer;
        };
    OnFrameFetched onFrameFetched = 
        [&frameBuffer, &nFetched, &fetchedFrameName, &fetchingSpawned](const boost::shared_ptr<IFrameFetcher>& fetcher, 
                                      const FrameInfo fi, int nFetchedFrames,
                                      int width, int height, const uint8_t* buffer)
        {
            boost::chrono::high_resolution_clock::time_point t = boost::chrono::high_resolution_clock::now();
            int d = boost::chrono::duration_cast<boost::chrono::milliseconds>(t - fetchingSpawned).count();

            EXPECT_EQ(frameBuffer, buffer);
            EXPECT_EQ(fetchedFrameName.toUri(), fi.ndnName_);

            GT_PRINTF("Fetching took %d ms, %d frames were fetched\n", 
                      d, nFetchedFrames);
            nFetched++;
            free(frameBuffer);
        };
    OnFetchFailure onFetchFailure = 
        [](const boost::shared_ptr<IFrameFetcher>&, std::string reason)
        {
            FAIL() << "Frame fetching failed: " << reason;
        };

    {
        // fetch random Delta frame
        Name dataName(streamPrefix);
        PacketNumber seqNo = rand()%deltaNo;
        dataName.append(thread)
                .append(NameComponents::NameComponentDelta)
                .appendSequenceNumber(seqNo);
        fetchedFrameName = dataName;
        
    #ifdef ENABLE_LOGGING
        ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDefault);
        LogInfo("") << "Will fetch " << dataName << std::endl;
        fetcher->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    #endif

        fetchingSpawned = boost::chrono::high_resolution_clock::now();
        fetcher->fetch(dataName, onBufferAllocate, onFrameFetched, onFetchFailure);
    }

    {
        // fetch random Key frame
        Name dataName(streamPrefix);
        PacketNumber seqNo = rand()%keyNo;
        dataName.append(thread)
                .append(NameComponents::NameComponentKey)
                .appendSequenceNumber(seqNo);
        fetchedFrameName = dataName;
        
    #ifdef ENABLE_LOGGING
        ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
        LogInfo("") << "Will fetch " << dataName << std::endl;
        fetcher->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    #endif

        fetchingSpawned = boost::chrono::high_resolution_clock::now();
        fetcher->fetch(dataName, onBufferAllocate, onFrameFetched, onFetchFailure);
    }

    EXPECT_EQ(2, nFetched);

    storage.reset();

    db_namespace::Options options;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

#if 1
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

    return msp;
}

TEST(TestPersistentStorage, TestFrameFetcherWithLocalStream)
{
#ifndef __ANDROID__
    std::string dbPath("/tmp/testdb");
#else
    std::string dbPath("/data/local/tmp/testdb");
#endif

#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
#endif

    boost::asio::io_service io_source;
    boost::shared_ptr<boost::asio::io_service::work> work_source(boost::make_shared<boost::asio::io_service::work>(io_source));
    boost::thread t_source([&io_source](){
        io_source.run();
    });

    int runTime = 1*5*1000;
    int width = 320, height = 240, bitrate = 1000;
    boost::shared_ptr<RawFrame> frame(boost::make_shared<ArgbFrame>(width,height));
    std::string testVideoSource = resources_path+"/test-source-320x240.argb";
    VideoSource source(io_source, testVideoSource, frame);
    MockExternalCapturer capturer;
    source.addCapturer(&capturer);

    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
    std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
    boost::shared_ptr<Face> publisherFace(boost::make_shared<ThreadsafeFace>(io_source));
    boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
    
    publisherFace->setCommandSigningInfo(*keyChain, certName(keyName(appPrefix)));
    boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(publisherFace.get());

    // setting up local stream
    MediaStreamSettings settings(io_source, getSampleVideoParams());
    settings.face_ = publisherFace.get();
    settings.keyChain_ = keyChain.get();
    settings.storagePath_ = dbPath;
    
    LocalVideoStream localStream(appPrefix, settings);

#ifdef ENABLE_LOGGING
    localStream.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    boost::function<int(const unsigned int,const unsigned int, unsigned char*, unsigned int)>
      incomingRawFrame =[&localStream](const unsigned int w,const unsigned int h, unsigned char* data, unsigned int size){
          EXPECT_NO_THROW(localStream.incomingArgbFrame(w, h, data, size));
          return 0;
      };
    EXPECT_CALL(capturer, incomingArgbFrame(320, 240, _, _))
        .WillRepeatedly(Invoke(incomingRawFrame));

    source.start(30);
    boost::this_thread::sleep_for(boost::chrono::milliseconds(runTime));
    work_source.reset();
    io_source.stop();
    t_source.join();

    // extract from db
    boost::shared_ptr<FrameFetcher> fetcher = boost::make_shared<FrameFetcher>(localStream.getStorage());
    
    uint8_t* frameBuffer = nullptr;
    int nFetched = 0;
    Name fetchedFrameName;
    boost::chrono::high_resolution_clock::time_point fetchingSpawned;

    OnBufferAllocate onBufferAllocate = 
        [&frameBuffer](const boost::shared_ptr<IFrameFetcher>& fetcher, int width, int height)->uint8_t*
        {
            frameBuffer = (uint8_t*)malloc(width*height*4);
            return frameBuffer;
        };

    OnFrameFetched onFrameFetched = 
        [&frameBuffer, &nFetched, &fetchedFrameName, &fetchingSpawned](const boost::shared_ptr<IFrameFetcher>& fetcher, 
                                      const FrameInfo fi, int nFetchedFrames,
                                      int width, int height, const uint8_t* buffer)
        {
            boost::chrono::high_resolution_clock::time_point t = boost::chrono::high_resolution_clock::now();
            int d = boost::chrono::duration_cast<boost::chrono::milliseconds>(t - fetchingSpawned).count();

            EXPECT_EQ(frameBuffer, buffer);
            EXPECT_EQ(fetchedFrameName.toUri(), fi.ndnName_);

            GT_PRINTF("Fetching took %d ms, %d frames were fetched\n", 
                      d, nFetchedFrames);
            nFetched++;
            free(frameBuffer);
        };
    OnFetchFailure onFetchFailure = 
        [](const boost::shared_ptr<IFrameFetcher>& ff, std::string reason)
        {
            FAIL() << "Frame fetching failed (" << ff->getName() <<"): " << reason;
        };

    {
        // fetch random Delta frame
        Name dataName(localStream.getPrefix());
        PacketNumber seqNo = rand()%60;
        dataName.append(localStream.getThreads()[0])
                .append(NameComponents::NameComponentDelta)
                .appendSequenceNumber(seqNo);
        fetchedFrameName = dataName;
        
    #ifdef ENABLE_LOGGING
        LogInfo("") << "Will fetch " << dataName << std::endl;
        fetcher->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    #endif

        fetchingSpawned = boost::chrono::high_resolution_clock::now();
        fetcher->fetch(dataName, onBufferAllocate, onFrameFetched, onFetchFailure);
    }

    {
        // fetch random Key frame
        Name dataName(localStream.getPrefix());
        PacketNumber seqNo = rand()%5;
        dataName.append(localStream.getThreads()[0])
                .append(NameComponents::NameComponentKey)
                .appendSequenceNumber(seqNo);
        fetchedFrameName = dataName;
        
    #ifdef ENABLE_LOGGING
        ndnlog::new_api::Logger::getLoggerPtr("")->setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
        LogInfo("") << "Will fetch " << dataName << std::endl;
        fetcher->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    #endif

        fetchingSpawned = boost::chrono::high_resolution_clock::now();
        fetcher->fetch(dataName, onBufferAllocate, onFrameFetched, onFetchFailure);
    }

    EXPECT_EQ(2, nFetched);

    db_namespace::Options options;
    db_namespace::DestroyDB(dbPath, options);
}
#endif

void handler(int sig) {
  void *array[10];
  size_t size;

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
#ifndef __ANDROID__
    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif

  exit(1);
}

int main(int argc, char **argv)
{
    signal(SIGABRT, handler);
    signal(SIGSEGV, handler);

    ::testing::InitGoogleTest(&argc, argv);

#ifdef HAVE_BOOST_FILESYSTEM
    boost::filesystem::path testPath(boost::filesystem::system_complete(argv[0]).remove_filename());
    test_path = testPath.string();

    boost::filesystem::path resPath(testPath);
    #ifndef __ANDROID__
    resPath /= boost::filesystem::path("..") /= 
               boost::filesystem::path("..") /=
               boost::filesystem::path("res");
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
    resources_path = test_path + "../..";
    #else
    resources_path = test_path;
    #endif
#endif

    return RUN_ALL_TESTS();
}
