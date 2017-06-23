// 
// test-async.cc
//
//  Created by Peter Gusev on 19 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>

#include "src/async.hpp"

using namespace ::testing;

class MockAsyncHelper {
public:
	MOCK_METHOD0(dispatch, void(void));
	MOCK_METHOD0(onCompletion, void(void));
};

TEST(TestAsync, TestDispatchAsync)
{
	MockAsyncHelper asyncHelper;
	boost::asio::io_service io;

	EXPECT_CALL(asyncHelper, dispatch())
		.Times(1);
	EXPECT_CALL(asyncHelper, onCompletion())
		.Times(1);

    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
        io.run();
	});

	ndnrtc::async::dispatchAsync(io, 
		boost::bind(&MockAsyncHelper::dispatch, &asyncHelper),
		boost::bind(&MockAsyncHelper::onCompletion, &asyncHelper));
    work.reset();
	t.join();
}

TEST(TestAsync, TestDispatchAsync2)
{
	boost::asio::io_service io;
	int dispValue = 0;
	boost::function<void(void)> disp = [&dispValue](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		dispValue = 1;
	};
	int compValue = 0;
	boost::function<void(void)> comp = [&compValue](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		compValue = 1;
	};

    boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
        io.run();
	});


	ndnrtc::async::dispatchAsync(io, disp, comp);
	
	EXPECT_EQ(0, dispValue);
	EXPECT_EQ(0, compValue);
	
	boost::this_thread::sleep_for(boost::chrono::milliseconds(150));
	EXPECT_NE(0, dispValue);
	EXPECT_NE(0, compValue);
    
    work.reset();
	t.join();
}

TEST(TestAsync, TestDispatchSync)
{
	boost::asio::io_service io;
	int dispValue = 0;
	boost::function<void(void)> disp = [&dispValue](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		dispValue = 1;
	};
	int compValue = 0;
	boost::function<void(void)> comp = [&compValue](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		compValue = 1;
	};

	boost::thread t([&io](){
		int n = 0;
		while (n < 500) 
		{
			io.run();
			boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
			n+=10;
		}
	});


	ndnrtc::async::dispatchSync(io, disp, comp);
	
	EXPECT_NE(0, dispValue);
	EXPECT_NE(0, compValue);
	
	t.join();
}

TEST(TestAsync, TestDispatchSyncReEntrant)
{
	boost::asio::io_service io;
	int dispValue = 0;
	boost::function<void(void)> disp = [&dispValue](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		dispValue = 1;
	};
	int compValue = 0;
	boost::function<void(void)> comp = [&compValue](){
		boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
		compValue = 1;
	};

	boost::thread t([&io](){
		int n = 0;
		while (n < 500) 
		{
			io.run();
			boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
			n+=10;
		}
	});

	boost::function<void(void)> callAsyncAgain = [&io, &comp, &disp](){
		ndnrtc::async::dispatchSync(io, comp, disp);
	};

	ndnrtc::async::dispatchSync(io, callAsyncAgain);

	EXPECT_EQ(1, dispValue);
	EXPECT_EQ(1, compValue);
	
	t.join();
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
