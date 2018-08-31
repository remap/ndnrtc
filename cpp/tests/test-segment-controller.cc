// 
// test-segment-controller.cc
//
//  Created by Peter Gusev on 04 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <ndn-cpp/network-nack.hpp>

#include "gtest/gtest.h"
#include "segment-controller.hpp"
#include "frame-data.hpp"

#include "mock-objects/segment-controller-observer-mock.hpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

// #define ENABLE_LOGGING

TEST(TestSegmentController, TestOnData)
{
	boost::shared_ptr<StatisticsStorage> sstorage(StatisticsStorage::createConsumerStatistics());
	boost::asio::io_service io;
	SegmentController controller(io, 500, sstorage);
    controller.setIsActive(true);

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
	controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	OnData onData = controller.getOnDataCallback();
	OnTimeout onTimeout = controller.getOnTimeoutCallback();

	std::string segmentPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/%00%00";
	boost::shared_ptr<Interest> i = boost::make_shared<Interest>(Name(segmentPrefix));
	boost::shared_ptr<Data> d = boost::make_shared<Data>(Name(segmentPrefix));

	MockSegmentControllerObserver o;
	controller.attach(&o);

	boost::function<void(const boost::shared_ptr<WireSegment>&)> checkSegment = [i,d]
		(const boost::shared_ptr<WireSegment>& seg)
		{
			EXPECT_EQ(d, seg->getData());
			EXPECT_EQ(i, seg->getInterest());
		};

	EXPECT_CALL(o, segmentArrived(_))
		.Times(1)
		.WillOnce(Invoke(checkSegment));

	onData(i, d);
	controller.detach(&o);

	EXPECT_CALL(o, segmentArrived(_))
		.Times(0);
	onData(i, d);

	EXPECT_EQ(2, (*sstorage)[Indicator::SegmentsReceivedNum]);
}

TEST(TestSegmentController, TestOnDataNotActive)
{
    boost::asio::io_service io;
    SegmentController controller(io, 500);
    controller.setIsActive(true);
    
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
    controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
    
    OnData onData = controller.getOnDataCallback();
    OnTimeout onTimeout = controller.getOnTimeoutCallback();
    
    std::string segmentPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/%00%00";
    boost::shared_ptr<Interest> i = boost::make_shared<Interest>(Name(segmentPrefix));
    boost::shared_ptr<Data> d = boost::make_shared<Data>(Name(segmentPrefix));
    
    MockSegmentControllerObserver o;
    controller.attach(&o);
    
    boost::function<void(const boost::shared_ptr<WireSegment>&)> checkSegment = [i,d]
    (const boost::shared_ptr<WireSegment>& seg)
    {
        EXPECT_EQ(d, seg->getData());
        EXPECT_EQ(i, seg->getInterest());
    };
    
    EXPECT_CALL(o, segmentArrived(_))
    .Times(1)
    .WillOnce(Invoke(checkSegment));
    
    onData(i, d);
    
    controller.setIsActive(false);
    EXPECT_CALL(o, segmentArrived(_))
    .Times(0);
    onData(i, d);
}

TEST(TestSegmentController, TestOnBadNamedData)
{
	boost::asio::io_service io;
	SegmentController controller(io, 500);
    controller.setIsActive(true);

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
	controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	OnData onData = controller.getOnDataCallback();
	OnTimeout onTimeout = controller.getOnTimeoutCallback();

	std::string segmentPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/%FE%07/%00%00";
	boost::shared_ptr<Interest> i = boost::make_shared<Interest>(Name(segmentPrefix));
	boost::shared_ptr<Data> d = boost::make_shared<Data>(Name(segmentPrefix));

	MockSegmentControllerObserver o;
	controller.attach(&o);

	EXPECT_CALL(o, segmentArrived(_))
		.Times(0);

	onData(i, d);
	controller.detach(&o);
}

TEST(TestSegmentController, TestOnTimeout)
{
    boost::shared_ptr<StatisticsStorage> sstorage(StatisticsStorage::createConsumerStatistics());
	boost::asio::io_service io;
	SegmentController controller(io, 500, sstorage);
    controller.setIsActive(true);

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
	controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	OnData onData = controller.getOnDataCallback();
	OnTimeout onTimeout = controller.getOnTimeoutCallback();

	std::string segmentPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/%00%00";
	boost::shared_ptr<Interest> i = boost::make_shared<Interest>(Name(segmentPrefix));
	boost::shared_ptr<Data> d = boost::make_shared<Data>(Name(segmentPrefix));

	MockSegmentControllerObserver o;
	controller.attach(&o);

	boost::function<void(const NamespaceInfo&, const boost::shared_ptr<const ndn::Interest> &)> checkTimeout = [i]
		(const NamespaceInfo& info, const boost::shared_ptr<const ndn::Interest> &)
		{
			EXPECT_EQ(info.getPrefix(), i->getName());
		};

	EXPECT_CALL(o, segmentRequestTimeout(_, _))
		.Times(1)
		.WillOnce(Invoke(checkTimeout));

	onTimeout(i);
	controller.detach(&o);
    
    EXPECT_EQ(1, (*sstorage)[Indicator::TimeoutsNum]);
}

TEST(TestSegmentController, TestOnNack)
{
    boost::shared_ptr<StatisticsStorage> sstorage(StatisticsStorage::createConsumerStatistics());
	boost::asio::io_service io;
	SegmentController controller(io, 500, sstorage);
    controller.setIsActive(true);

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
	controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

	OnData onData = controller.getOnDataCallback();
	OnNetworkNack onNack = controller.getOnNetworkNackCallback();

	std::string segmentPrefix = "/ndn/edu/ucla/remap/peter/ndncon/instance1/ndnrtc/%FD%02/video/camera/hi/d/%FE%07/%00%00";
	boost::shared_ptr<Interest> i = boost::make_shared<Interest>(Name(segmentPrefix));
	boost::shared_ptr<Data> d = boost::make_shared<Data>(Name(segmentPrefix));
	boost::shared_ptr<NetworkNack> nack = boost::make_shared<NetworkNack>();

	MockSegmentControllerObserver o;
	controller.attach(&o);

	boost::function<void(const NamespaceInfo&, int reason, const boost::shared_ptr<const ndn::Interest> &)> checkNack = [i]
		(const NamespaceInfo& info, int reason, const boost::shared_ptr<const ndn::Interest> &)
		{
			EXPECT_EQ(info.getPrefix(), i->getName());
		};

	EXPECT_CALL(o, segmentNack(_,ndn_NetworkNackReason_NONE, _))
		.Times(1)
		.WillOnce(Invoke(checkNack));

	onNack(i, nack);
	controller.detach(&o);
    
    EXPECT_EQ(1, (*sstorage)[Indicator::NacksNum]);
}

TEST(TestSegmentController, TestStarvation)
{
	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io));
	boost::thread t([&io](){
		io.run();
	});

	MockSegmentControllerObserver o;
	EXPECT_CALL(o, segmentStarvation())
		.Times(1);

	{
		SegmentController controller(io, 500);
        controller.setIsActive(true);

#ifdef ENABLE_LOGGING
		ndnlog::new_api::Logger::initAsyncLogging();
		ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
		controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

		controller.attach(&o);
		boost::this_thread::sleep_for(boost::chrono::milliseconds(1050));
	}
	work.reset();
	t.join();
}

TEST(TestSegmentController, TestStarvationNotActive)
{
    boost::asio::io_service io;
    boost::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io));
    boost::thread t([&io](){
        io.run();
    });
    
    MockSegmentControllerObserver o;
    EXPECT_CALL(o, segmentStarvation())
    .Times(0);
    
    {
        SegmentController controller(io, 200);
        
#ifdef ENABLE_LOGGING
        ndnlog::new_api::Logger::initAsyncLogging();
        ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelDebug);
        controller.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
        
        controller.attach(&o);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(400));
        
        EXPECT_CALL(o, segmentStarvation())
        .Times(1);
        controller.setIsActive(true);
        boost::this_thread::sleep_for(boost::chrono::milliseconds(400));
    }
    work.reset();
    t.join();
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
