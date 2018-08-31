// 
// test-stat-collector.cc
//
//  Created by Peter Gusev on 10 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <boost/asio.hpp>
#include <boost/assign.hpp>
#include <boost/make_shared.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#include <client/src/stat-collector.hpp>
#include "tests-helpers.hpp"
#include "mock-objects/stream-mock.hpp"

#define PREFIX string("/ndn/edu/ucla/remap/ndnrtc/user")
#define CLIENT1 string("clientA")
#define CLIENT1_SESSION_PREFIX (PREFIX + "/" + CLIENT1)
#define STREAM_AUDIO string("sound")
#define STREAM_VIDEO1 string("camera")
#define STREAM_VIDEO2 string("desktop")
#define STREAM_VIDEO3 string("camera2")
#define CLIENT1_STREAM_PREFIX_AUDIO (CLIENT1_SESSION_PREFIX + "/streams/" + STREAM_AUDIO)
#define CLIENT1_STREAM_PREFIX_VIDEO1 (CLIENT1_SESSION_PREFIX + "/streams/" + STREAM_VIDEO1)
#define CLIENT1_STREAM_PREFIX_VIDEO2 (CLIENT1_SESSION_PREFIX + "/streams/" + STREAM_VIDEO2)
#define CLIENT1_STREAM_PREFIX_VIDEO3 (CLIENT1_SESSION_PREFIX + "/streams/" + STREAM_VIDEO3)

using namespace testing;
using namespace boost::asio;
using namespace ndnrtc;
using namespace std;
using namespace ndnrtc::statistics;
#if 0
TEST(TestCsvFormatter, TestCsvDefault)
{
	map<string, double> metrics = boost::assign::map_list_of<string, double> 
		("timestamp", 1457733705984)
		("rebuf", 0.)
		("drdEst", 120.)
		("jitterPlay", 75.6752f)
		("lambdaD", 4);
	vector<string> order=boost::assign::list_of("timestamp")("rebuf")("drdEst")
		("jitterPlay")("lambdaD");
	CsvFormatter cf(2);

	EXPECT_EQ("timestamp\trebuf\tdrdEst\tjitterPlay\tlambdaD\n", cf.getHeader(metrics, order));
	EXPECT_EQ("1457733705984.00\t0.00\t120.00\t75.68\t4.00", cf(metrics, order));
}

TEST(TestCsvFormatter, TestCsvHigherPrecision)
{
	map<string, double> metrics = boost::assign::map_list_of<string, double> 
		("timestamp", 1457733705984)
		("rebuf", 0.)
		("drdEst", 120.1234)
		("jitterPlay", 75.6752)
		("lambdaD", 4);
	vector<string> order=boost::assign::list_of("timestamp")("rebuf")("drdEst")
		("jitterPlay")("lambdaD");
	CsvFormatter cf(4);

	EXPECT_EQ("1457733705984.0000\t0.0000\t120.1234\t75.6752\t4.0000", cf(metrics, order));
}

TEST(TestCsvFormatter, TestCsvComaSeparator)
{
	map<string, double> metrics = boost::assign::map_list_of<string, double> 
		("timestamp", 1457733705984)
		("rebuf", 0.)
		("drdEst", 120.)
		("jitterPlay", 75.6752f)
		("lambdaD", 4);
	vector<string> order=boost::assign::list_of("timestamp")("rebuf")("drdEst")
		("jitterPlay")("lambdaD");
	CsvFormatter cf(2, ",");

	EXPECT_EQ("timestamp,rebuf,drdEst,jitterPlay,lambdaD\n", cf.getHeader(metrics, order));
	EXPECT_EQ("1457733705984.00,0.00,120.00,75.68,4.00", cf(metrics, order));
}

TEST(TestFormatter, TestJson)
{
	map<string, double> metrics = boost::assign::map_list_of<string, double> 
		("timestamp", 1457733705984)
		("rebuf", 0.)
		("drdEst", 120.)
		("jitterPlay", 75.6752f)
		("lambdaD", 4);
	vector<string> order=boost::assign::list_of("timestamp")("lambdaD")
		("jitterPlay")("drdEst")("rebuf");
	JsonFormatter cf(2, false);

	EXPECT_EQ("", cf.getHeader(metrics, order));
	EXPECT_EQ("{\n"
		"\t\"timestamp\": 1457733705984.00,\n"
		"\t\"lambdaD\": 4.00,\n"
		"\t\"jitterPlay\": 75.68,\n"
		"\t\"drdEst\": 120.00,\n"
		"\t\"rebuf\": 0.00\n"
		"}", cf(metrics, order));
}

TEST(TestFormatter, TestJsonCompact)
{
	map<string, double> metrics = boost::assign::map_list_of<string, double> 
		("timestamp", 1457733705984)
		("rebuf", 0.)
		("drdEst", 120.)
		("jitterPlay", 75.6752f)
		("lambdaD", 4);
	vector<string> order=boost::assign::list_of("timestamp")("lambdaD")
		("jitterPlay")("drdEst")("rebuf");
	JsonFormatter cf(2, true);

	EXPECT_EQ("", cf.getHeader(metrics, order));
	EXPECT_EQ("{\"timestamp\":1457733705984.00,\"lambdaD\":4.00,"
		"\"jitterPlay\":75.68,\"drdEst\":120.00,\"rebuf\":0.00}", 
		cf(metrics, order));
}

TEST(TestStatWriter, TestOutput)
{
	StatGatheringParams sgp("buffer");

	sgp.addStat("jitterPlay");
	sgp.addStat("jitterTar");
	sgp.addStat("drdPrime");

	CsvFormatter cf;
	stringstream ss;
	StatWriter sw(sgp, &ss, boost::make_shared<CsvFormatter>(cf));

	StatisticsStorage::StatRepo repo;
	repo[Indicator::Timestamp] = 1457733705984;
	repo[Indicator::BufferPlayableSize] = 76;
	repo[Indicator::BufferTargetSize] = 50;
	repo[Indicator::DrdCachedEstimation] = 200;

	sw.writeStats(repo);
	sw.flush();

	EXPECT_EQ("timestamp\tjitterPlay\tjitterTar\tdrdPrime\n"
		"1457733705984.00\t76.00\t50.00\t200.00\n",
		ss.str());
}

TEST(TestStatWriter, TestOutputWrongStatName)
{
	StatGatheringParams sgp("buffer");

	sgp.addStat("jitterPlay");
	sgp.addStat("jitterTarget"); // stat name error 
	sgp.addStat("drdPrime");

	CsvFormatter cf;
	stringstream ss;
	StatWriter sw(sgp, &ss, boost::make_shared<CsvFormatter>(cf));

	StatisticsStorage::StatRepo repo;
	repo[Indicator::Timestamp] = 1457733705984;
	repo[Indicator::BufferPlayableSize] = 76;
	repo[Indicator::BufferTargetSize] = 50;
	repo[Indicator::DrdCachedEstimation] = 200;

	sw.writeStats(repo);
	sw.flush();

	EXPECT_EQ("timestamp\tjitterPlay\tjitterTarget\tdrdPrime\n"
		"1457733705984.00\t76.00\t0.00\t200.00\n",
		ss.str());
}

TEST(TestFileStatWriter, TestOuput)
{
	StatGatheringParams sgp("buffer");

	sgp.addStat("jitterPlay");
	sgp.addStat("jitterTarget"); // stat name error 
	sgp.addStat("drdPrime");

	stringstream ss;
	string fileName = "tests/test.stat";
	StatFileWriter sw(sgp, boost::make_shared<CsvFormatter>(), fileName);

	StatisticsStorage::StatRepo repo;
	repo[Indicator::Timestamp] = 1457733705984;
	repo[Indicator::BufferPlayableSize] = 76;
	repo[Indicator::BufferTargetSize] = 50;
	repo[Indicator::DrdCachedEstimation] = 200;

	sw.writeStats(repo);
	sw.flush();

	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	std::ifstream t(fileName.c_str());
	std::string statFileContents((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());

	EXPECT_EQ("timestamp\tjitterPlay\tjitterTarget\tdrdPrime\n"
		"1457733705984.00\t76.00\t0.00\t200.00\n",
		statFileContents);
	remove(fileName.c_str());
}

TEST(TestFileStatWriter, TestJsonOuput)
{
	StatGatheringParams sgp("buffer");

	sgp.addStat("jitterPlay");
	sgp.addStat("jitterTarget"); // stat name error 
	sgp.addStat("drdPrime");

	stringstream ss;
	string fileName = "tests/test.stat";
	StatFileWriter sw(sgp, boost::make_shared<JsonFormatter>(), fileName);

	StatisticsStorage::StatRepo repo;
	repo[Indicator::Timestamp] = 1457733705984;
	repo[Indicator::BufferPlayableSize] = 76;
	repo[Indicator::BufferTargetSize] = 50;
	repo[Indicator::DrdCachedEstimation] = 200;

	sw.writeStats(repo);
	sw.flush();

	ASSERT_TRUE(std::ifstream(fileName.c_str()).good());

	std::ifstream t(fileName.c_str());
	std::string statFileContents((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());

	EXPECT_EQ("{\"timestamp\":1457733705984.00,"
		"\"jitterPlay\":76.00,\"jitterTarget\":0.00,\"drdPrime\":200.00}\n",
		statFileContents);
	remove(fileName.c_str());
}

TEST(TestStatCollector, TestCreate)
{
	io_service io;
	StatCollector sc(io);
}

TEST(TestStatCollector, TestAddStreams)
{
	io_service io;
	StatCollector sc(io);

	EXPECT_EQ(0, sc.getStreamsNumber());

	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_AUDIO));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO1));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO2));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO3));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO3));
		EXPECT_ANY_THROW(sc.addStream(s));
	}

	EXPECT_EQ(4, sc.getStreamsNumber());
	EXPECT_EQ(0, sc.getWritersNumber());
}

TEST(TestStatCollector, TestRemoveAllStreams)
{
	io_service io;
	StatCollector sc(io);

	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_AUDIO));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO1));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO2));
		sc.addStream(s);
	}
	{
		boost::shared_ptr<MockStream> s(boost::make_shared<MockStream>());
		EXPECT_CALL(*s, getPrefix())
			.Times(AtLeast(1))
			.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO3));
		sc.addStream(s);
	}
	sc.removeAllStreams();

	EXPECT_EQ(0, sc.getStreamsNumber());
}

TEST(TestStatCollector, TestRemoveStreams)
{
	io_service io;
	StatCollector sc(io);

	boost::shared_ptr<MockStream> s1(boost::make_shared<MockStream>());
	EXPECT_CALL(*s1, getPrefix())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_AUDIO));
	sc.addStream(s1);

	boost::shared_ptr<MockStream> s2(boost::make_shared<MockStream>());
	EXPECT_CALL(*s2, getPrefix())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO1));
	sc.addStream(s2);

	boost::shared_ptr<MockStream> s3(boost::make_shared<MockStream>());
	EXPECT_CALL(*s3, getPrefix())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO2));
	sc.addStream(s3);

	boost::shared_ptr<MockStream> s4(boost::make_shared<MockStream>());
	EXPECT_CALL(*s4, getPrefix())
		.Times(AtLeast(1))
		.WillRepeatedly(Return(CLIENT1_STREAM_PREFIX_VIDEO3));
	sc.addStream(s4);

	sc.removeStream(s1);
	EXPECT_EQ(3, sc.getStreamsNumber());

	sc.removeStream(s3);
	EXPECT_EQ(2, sc.getStreamsNumber());

	sc.removeStream(s3);
	EXPECT_EQ(2, sc.getStreamsNumber());
}
#endif
#if 0
TEST(TestStatCollector, TestGathering)
{
	MockNdnRtcLibrary ndnrtcLib;
	io_service io;
	StatCollector sc(io, &ndnrtcLib);

	sc.addStream(CLIENT1_STREAM_PREFIX_AUDIO);
	sc.addStream(CLIENT1_STREAM_PREFIX_VIDEO1);

    boost::shared_ptr<boost::asio::io_service::work> work;
    boost::asio::deadline_timer runTimer(io);
    std::vector<StatGatheringParams> statParams;

    {
    	StatGatheringParams sgp("buffer");
		sgp.addStat("jitterPlay");
		sgp.addStat("jitterTar");
		sgp.addStat("drdPrime");
		statParams.push_back(sgp);
	}
	{
    	StatGatheringParams sgp("playback");
		sgp.addStat("latEst");
		sgp.addStat("framesPlayed");
		sgp.addStat("skipInc");
		statParams.push_back(sgp);
	}

	boost::shared_ptr<StatisticsStorage> sampleStats = 
		boost::shared_ptr<StatisticsStorage>(StatisticsStorage::createConsumerStatistics());
	(*sampleStats)[Indicator::Timestamp] = 1457733705984;
	(*sampleStats)[Indicator::BufferPlayableSize] = 76;
	(*sampleStats)[Indicator::BufferTargetSize] = 50;
	(*sampleStats)[Indicator::RttPrime] = 200;
	(*sampleStats)[Indicator::LatencyEstimated] = 0.2;
	(*sampleStats)[Indicator::PlayedNum] = 1000;
	(*sampleStats)[Indicator::SkippedIncompleteNum] = 2;

	std::srand(0);

	auto statGenerator = [&](const std::string &prefix){
			(*sampleStats)[Indicator::Timestamp] += 100;
			(*sampleStats)[Indicator::BufferPlayableSize] += std::rand()%10-5;
			(*sampleStats)[Indicator::BufferTargetSize] = 50;
			(*sampleStats)[Indicator::RttPrime] += std::rand()%40-20;
			(*sampleStats)[Indicator::LatencyEstimated] += (std::rand()%100-50)/1000;
			(*sampleStats)[Indicator::PlayedNum] = 1000+3;
			(*sampleStats)[Indicator::SkippedIncompleteNum] = 2;
			return *sampleStats;
		};

	EXPECT_CALL(ndnrtcLib, getRemoteStreamStatistics(CLIENT1_STREAM_PREFIX_AUDIO))
		.Times(AtLeast(10))
		.WillRepeatedly(Invoke(statGenerator));
	EXPECT_CALL(ndnrtcLib, getRemoteStreamStatistics(CLIENT1_STREAM_PREFIX_VIDEO1))
		.Times(AtLeast(10))
		.WillRepeatedly(Invoke(statGenerator));

    sc.startCollecting(100, "/tmp", statParams);
    
    runTimer.expires_from_now(boost::posix_time::milliseconds(1100));
    runTimer.async_wait([&](const boost::system::error_code& ){
        work.reset();
        sc.stop();
    });
    work.reset(new boost::asio::io_service::work(io));
    io.run();
    io.stop();

	ASSERT_TRUE(std::ifstream(string("/tmp/buffer-"+CLIENT1+"-"+STREAM_AUDIO+".stat").c_str()).good());
	ASSERT_TRUE(std::ifstream(string("/tmp/buffer-"+CLIENT1+"-"+STREAM_VIDEO1+".stat").c_str()).good());
	ASSERT_TRUE(std::ifstream(string("/tmp/playback-"+CLIENT1+"-"+STREAM_AUDIO+".stat").c_str()).good());
	ASSERT_TRUE(std::ifstream(string("/tmp/playback-"+CLIENT1+"-"+STREAM_VIDEO1+".stat").c_str()).good());

	std::ifstream t("/tmp/buffer-clientA-sound.stat");
	std::string statFileContents((std::istreambuf_iterator<char>(t)),
                 std::istreambuf_iterator<char>());
	std::vector<std::string> lines;
	boost::split(lines, statFileContents, boost::is_any_of("\n"), boost::token_compress_on);
	EXPECT_GE(lines.size(), 9);

    remove(string("/tmp/buffer-"+CLIENT1+"-"+STREAM_AUDIO+".stat").c_str());
    remove(string("/tmp/buffer-"+CLIENT1+"-"+STREAM_VIDEO1+".stat").c_str());
    remove(string("/tmp/playback-"+CLIENT1+"-"+STREAM_AUDIO+".stat").c_str());
    remove(string("/tmp/playback-"+CLIENT1+"-"+STREAM_VIDEO1+".stat").c_str());
}
#endif
//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
