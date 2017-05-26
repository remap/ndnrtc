// 
// periodic.cpp
//
//  Created by Peter Gusev on 05 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "periodic.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/chrono.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

#if BOOST_ASIO_HAS_STD_CHRONO

namespace lib_chrono=std::chrono;

#else

namespace lib_chrono=boost::chrono;

#endif


using namespace ndnrtc;

namespace ndnrtc {
	class PeriodicImpl : public boost::enable_shared_from_this<PeriodicImpl> 
	{
	public:
		PeriodicImpl(boost::asio::io_service& io);
		~PeriodicImpl();

		void setupTimer(unsigned int intervalMs);
		void fire(const boost::system::error_code& e);
		void cancel();

		boost::atomic<bool> isRunning_;
		boost::asio::io_service& io_;
		boost::function<unsigned int(void)> workFunc_;
		boost::asio::steady_timer timer_;
	};
}

Periodic::Periodic(boost::asio::io_service& io):
pimpl_(boost::make_shared<PeriodicImpl>(io))
{
}

Periodic::~Periodic()
{
	pimpl_->cancel();
}

void 
Periodic::setupInvocation(unsigned int intervalMs, 
	boost::function<unsigned int(void)> callback)
{
	if (!pimpl_->isRunning_)
	{
		pimpl_->isRunning_ = true;
		pimpl_->workFunc_ = callback;
		pimpl_->setupTimer(intervalMs);
	}
	else
		throw std::runtime_error("Periodic invocation is already running");
}

void
Periodic::cancelInvocation()
{
	pimpl_->cancel();
}

bool
Periodic::isPeriodicInvocationSet() const
{
	return pimpl_->isRunning_;
}

//******************************************************************************
PeriodicImpl::PeriodicImpl(boost::asio::io_service& io):
isRunning_(false),io_(io),timer_(io_)
{
}

PeriodicImpl::~PeriodicImpl()
{
}

void
PeriodicImpl::setupTimer(unsigned int intervalMs)
{
	timer_.expires_from_now(lib_chrono::milliseconds(intervalMs));
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
			else 
                cancel();
		}
	}
}

void 
PeriodicImpl::cancel()
{
	isRunning_ = false;
	timer_.cancel();
    workFunc_ = boost::function<unsigned int(void)>();
}
