//
//  playout.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include "playout.hpp"
#include "playout-impl.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

Playout::Playout(boost::asio::io_service& io,
        const boost::shared_ptr<IPlaybackQueue>& queue,
        const boost::shared_ptr<StatStorage> statStorage):
pimpl_(boost::make_shared<PlayoutImpl>(io, queue, statStorage)){}

void Playout::start(unsigned int fastForwardMs) { pimpl_->start(fastForwardMs); }
void Playout::stop() { pimpl_->stop(); }
void Playout::addAdjustment(int64_t adjMs) { pimpl_->addAdjustment(adjMs); } 
void Playout::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger) { pimpl_->setLogger(logger); }
void Playout::setDescription(const std::string& desc) { pimpl_->setDescription(desc); }
bool Playout::isRunning() const { return pimpl_->isRunning(); }
void Playout::attach(IPlayoutObserver* observer) { pimpl_->attach(observer); }
void Playout::detach(IPlayoutObserver* observer) { pimpl_->detach(observer); }

PlayoutImpl* Playout::pimpl() { return pimpl_.get(); }
PlayoutImpl* Playout::pimpl() const { return pimpl_.get(); }
