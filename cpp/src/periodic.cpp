// 
// periodic.cpp
//
//  Created by Peter Gusev on 05 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "periodic.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/chrono.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

using namespace ndnrtc;

namespace ndnrtc {
	class PeriodicImpl : public boost::enable_shared_from_this<PeriodicImpl> 
	{
	public:
		PeriodicImpl(boost::asio::io_service& io, unsigned int periodMs,
			boost::function<unsigned int(void)> workFunc);
		~PeriodicImpl();

		void setupTimer(unsigned int intervalMs);
		void fire(const boost::system::error_code& e);
		void cancel();

		boost::atomic<bool> isRunning_;
		unsigned int periodMs_;
		boost::asio::io_service& io_;
		boost::function<unsigned int(void)> workFunc_;
		boost::asio::steady_timer timer_;
	};
}

Periodic::Periodic(boost::asio::io_service& io, unsigned int periodMs):
pimpl_(boost::make_shared<PeriodicImpl>(io, periodMs, 
	boost::bind(&Periodic::periodicInvocation, this)))
{
	pimpl_->setupTimer(periodMs);
}

Periodic::~Periodic()
{
	pimpl_->cancel();
}

//******************************************************************************
PeriodicImpl::PeriodicImpl(boost::asio::io_service& io, unsigned int periodMs,
	boost::function<unsigned int(void)> workFunc):
isRunning_(true),io_(io),periodMs_(periodMs),workFunc_(workFunc),timer_(io_)
{
}

PeriodicImpl::~PeriodicImpl()
{
}

void
PeriodicImpl::setupTimer(unsigned int intervalMs)
{
	timer_.expires_from_now(boost::chrono::milliseconds(intervalMs));
	timer_.async_wait(boost::bind(&PeriodicImpl::fire, shared_from_this(), 
		boost::asio::placeholders::error));
}

void 
PeriodicImpl::fire(const boost::system::error_code& e)
{
	if (!e)
	{
		if (isRunning_)
		{
			unsigned int nextIntervalMs = workFunc_();
			if (nextIntervalMs)
				setupTimer(nextIntervalMs);
		}
	}
}

void 
PeriodicImpl::cancel()
{
	isRunning_ = false;
	timer_.cancel();
}
