// 
// media-stream-base.cpp
//
//  Created by Peter Gusev on 21 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/thread/lock_guard.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "media-stream-base.h"
#include "name-components.h"
#include "async.h"


#define META_CHECK_INTERVAL_MS 1000

using namespace ndnrtc;
using namespace std;
using namespace ndn;

// how often is thread meta information published
const unsigned int MediaStreamBase::MetaCheckIntervalMs = META_CHECK_INTERVAL_MS;

MediaStreamBase::MediaStreamBase(const std::string& basePrefix, 
	const MediaStreamSettings& settings):
Periodic(settings.faceIo_),
metaVersion_(0),
settings_(settings),
streamPrefix_(NameComponents::streamPrefix(settings.params_.type_, basePrefix)),
cache_(boost::make_shared<ndn::MemoryContentCache>(settings_.face_)),
statStorage_(statistics::StatisticsStorage::createProducerStatistics())
{
	assert(settings_.face_);
	assert(settings_.keyChain_);

	streamPrefix_.append(Name(settings_.params_.streamName_));
    cache_->setInterestFilter(streamPrefix_, cache_->getStorePendingInterest());

	PublisherSettings ps;
    ps.sign_ = true; 	// it's ok to sign every packet as data publisher 
    					// is used for low-rate data (max 10fps), 
	ps.keyChain_  = settings_.keyChain_;
	ps.memoryCache_ = cache_.get();
	ps.segmentWireLength_ = MAX_NDN_PACKET_SIZE;	// it's ok to rely on link-layer fragmenting
													// because data is low-rate
	ps.freshnessPeriodMs_ = settings_.params_.producerParams_.freshnessMs_;
    ps.statStorage_ = statStorage_.get();
	
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

statistics::StatisticsStorage 
MediaStreamBase::getStatistics() const
{
	return *statStorage_;
}

void 
MediaStreamBase::publishMeta()
{
	boost::shared_ptr<MediaStreamMeta> meta(boost::make_shared<MediaStreamMeta>(getThreads()));
	// don't need to synchronize unless publishMeta will be called 
	// from places others than addThread/removeThread or outer class' 
	// constructor LocalVideoStream/LocalAudioStream
	
	Name metaName(streamPrefix_);
	metaName.append(NameComponents::NameComponentMeta).appendVersion(++metaVersion_);

	boost::shared_ptr<MediaStreamBase> me = boost::static_pointer_cast<MediaStreamBase>(shared_from_this());
	async::dispatchAsync(settings_.faceIo_, [me, metaName, meta](){
		me->dataPublisher_->publish(metaName, *meta);
	});
	
	LogDebugC << "published stream meta " << metaName << std::endl;
}

unsigned int MediaStreamBase::periodicInvocation()
{
	if (checkMeta())
		return META_CHECK_INTERVAL_MS;
	return 0;
}
