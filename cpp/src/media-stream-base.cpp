// 
// media-stream-base.cpp
//
//  Created by Peter Gusev on 21 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/smart_ptr/shared_ptr.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "media-stream-base.h"
#include "name-components.h"
#include "async.h"

using namespace ndnrtc;
using namespace std;
using namespace ndn;

MediaStreamBase::MediaStreamBase(const std::string& basePrefix, 
	const MediaStreamSettings& settings):
metaVersion_(0),
settings_(settings),
streamPrefix_(NameComponents::streamPrefix(settings.params_.type_, basePrefix)),
cache_(boost::make_shared<ndn::MemoryContentCache>(settings_.face_))
{
	streamPrefix_.append(Name(settings_.params_.streamName_));

	PublisherSettings ps;
	ps.keyChain_  = settings_.keyChain_;
	ps.memoryCache_ = cache_.get();
	ps.segmentWireLength_ = settings_.params_.producerParams_.segmentSize_;
	ps.freshnessPeriodMs_ = settings_.params_.producerParams_.freshnessMs_;
	
	dataPublisher_ = boost::make_shared<CommonPacketPublisher>(ps);
	dataPublisher_->setDescription("data-publisher-"+settings_.params_.streamName_);
}

MediaStreamBase::~MediaStreamBase()
{
	// cleanup
}

void
MediaStreamBase::addThread(const MediaThreadParams* params)
{
	add(params);
	publishMeta();
}

void 
MediaStreamBase::removeThread(const string& threadName)
{
	remove(threadName);
	publishMeta();
}

void 
MediaStreamBase::publishMeta()
{
	boost::shared_ptr<MediaStreamMeta> meta(boost::make_shared<MediaStreamMeta>());
	for (auto t:getThreads()) meta->addThread(t);
	Name metaName(streamPrefix_);
	metaName.append(NameComponents::NameComponentMeta).appendVersion(++metaVersion_);

	boost::shared_ptr<MediaStreamBase> me = boost::static_pointer_cast<MediaStreamBase>(shared_from_this());
	async::dispatchAsync(settings_.faceIo_, [me, metaName, meta](){
		me->dataPublisher_->publish(metaName, *meta);
	});

	LogDebugC << "published stream meta " << metaName << std::endl;
}
