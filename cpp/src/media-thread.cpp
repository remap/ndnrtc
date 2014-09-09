//
//  media-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "media-thread.h"
#include "ndnrtc-namespace.h"
#include "ndnrtc-utils.h"
#include "ndnrtc-debug.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc::new_api;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
MediaThread::MediaThread():
NdnRtcComponent(),
packetNo_(0),
pitCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection())
{
    packetRateMeter_ = NdnRtcUtils::setupFrequencyMeter(4);
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(5);
}

MediaThread::~MediaThread()
{
    stop();
    
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
}

//******************************************************************************
//******************************************************************************
#pragma mark - static


//******************************************************************************
#pragma mark - public
int MediaThread::init(const MediaThreadSettings &settings)
{
    settings_ = settings;
    
    if (settings_.useCache_)
        memCache_.reset(new MemoryContentCache(settings_.faceProcessor_->getFaceWrapper()->getFace().get()));
    
    try {
        registerPrefix(settings_.prefix_);
    }
    catch (std::exception &e)
    {
        return notifyError(RESULT_ERR,
                           "got error from ndn library while registering prefix: %s",
                           e.what());
    }
    
    segSizeNoHeader_ = settings_.segmentSize_ - SegmentData::getHeaderSize();
    
    return RESULT_OK;
}

void MediaThread::stop()
{
}

//******************************************************************************
#pragma mark - private
int MediaThread::publishPacket(PacketData &packetData,
                               const Name& packetPrefix,
                               PacketNumber packetNo,
                               PrefixMetaInfo prefixMeta,
                               double captureTimestamp)
{
    if (!settings_.faceProcessor_->getTransport()->getIsConnected())
        return notifyError(-1, "transport is not connected");
    
    Name prefix = packetPrefix;
    Segmentizer::SegmentList segments;
    
    if (RESULT_FAIL(Segmentizer::segmentize(packetData, segments, segSizeNoHeader_)))
        return notifyError(-1, "packet segmentation failed");
    
    try {
        // update metadata for the packet
        PacketData::PacketMetadata metadata = { NdnRtcUtils::currentFrequencyMeterValue(packetRateMeter_),
            NdnRtcUtils::millisecondTimestamp(),
            captureTimestamp};
        
        packetData.setMetadata(metadata);
        
        Name metaSuffix = PrefixMetaInfo::toName(prefixMeta);
        
        for (Segmentizer::SegmentList::iterator it = segments.begin();
             it != segments.end(); ++it)
        {
            // add segment #
            Name segmentName = prefix;
            segmentName.appendSegment(it-segments.begin());
            
            // lookup for pending interests and construct metaifno accordingly
            SegmentData::SegmentMetaInfo meta = {0,0,0};
            bool pitHit = (lookupPrefixInPit(segmentName, meta) != 0);
            
            // add name suffix meta info
            segmentName.append(metaSuffix);
            
            // pack into network data
            SegmentData segmentData(it->getDataPtr(), it->getPayloadSize(), meta);
            
            Data ndnData(segmentName);
            ndnData.getMetaInfo().setFreshnessPeriod(settings_.dataFreshnessMs_);
            ndnData.setContent(segmentData.getData(), segmentData.getLength());
            
            settings_.keyChain_->sign(ndnData, settings_.certificateName_);
            
            if (settings_.useCache_ && !pitHit)
            {
                // according to http://named-data.net/doc/ndn-ccl-api/memory-content-cache.html#memorycontentcache-registerprefix-method
                // adding content should be synchronized with the processEvents
                // call
                faceProcessor_->getFaceWrapper()->synchronizeStart();
                memCache_->add(ndnData);
                faceProcessor_->getFaceWrapper()->synchronizeStop();
                
                LogTraceC
                << "added to cache " << segmentName << " "
                << ndnData.getContent().size() << " bytes" << std::endl;
            }
            else
            {
                SignedBlob encodedData = ndnData.wireEncode();
                faceProcessor_->getTransport()->send(*encodedData);
                
                LogTraceC
                << "published " << segmentName << " "
                << ndnData.getContent().size() << " bytes" << std::endl;
            }
            
#if 0   // enable this if you want measuring outgoing bitrate w/o ndn overhead
            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                               ndnData.getContent().size());
#else   // enable this if you want measuring outgoing bitrate with ndn overhead
            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                               ndnData.getDefaultWireEncoding().size());
#endif
        }
    }
    catch (std::exception &e)
    {
        return notifyError(RESULT_ERR,
                           "got error from ndn library while sending data: %s",
                           e.what());
    }
    
    return segments.size();
}

void MediaThread::registerPrefix(const Name& prefix)
{
    if (settings_.useCache_)
    {
        memCache_->registerPrefix(prefix,
                                  bind(&MediaThread::onRegisterFailed,
                                       this, _1),
                                  bind(&MediaThread::onInterest,
                                       this, _1, _2, _3));
    }
    else
    {
        uint64_t prefixId = faceProcessor_->getFaceWrapper()->registerPrefix(prefix,
                                                                             bind(&MediaThread::onInterest,
                                                                                  this, _1, _2, _3),
                                                                             bind(&MediaThread::onRegisterFailed,
                                                                                  this, _1));
        if (prefixId != 0)
            LogTraceC << "registered prefix " << prefix << std::endl;
    }
}

void MediaThread::onInterest(const shared_ptr<const Name>& prefix,
                             const shared_ptr<const Interest>& interest,
                             ndn::Transport& transport)
{
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(interest->getName());
    
    if (packetNo >= packetNo_)
    {
        addToPit(interest);
    }
    
    LogTraceC << "incoming interest for " << interest->getName()
    << ((packetNo >= packetNo_ || packetNo == -1)?" (new)":" (old)") << std::endl;
}

void MediaThread::onRegisterFailed(const shared_ptr<const Name>& prefix)
{
    if (hasCallback())
        getCallback()->onMediaThreadRegistrationFailed();
    
    notifyError(-1, "registration on %s has failed", prefix->toUri().c_str());
}

void MediaThread::addToPit(const shared_ptr<const ndn::Interest> &interest)
{
    const Name& name = interest->getName();
    PitEntry pitEntry;
    
    pitEntry.arrivalTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    pitEntry.interest_ = interest;
    
    {
        webrtc::CriticalSectionScoped scopedCs_(&pitCs_);
        
        if (pit_.find(name) != pit_.end())
        {
            LogTraceC << "pit exists " << name << std::endl;
        }
        else
        {
            pit_[name] = pitEntry;
            LogTraceC << "new pit entry " << name << " " << pit_.size() << std::endl;
        }
    }
}

int MediaThread::lookupPrefixInPit(const ndn::Name &prefix,
                                   SegmentData::SegmentMetaInfo &metaInfo)
{
    webrtc::CriticalSectionScoped scopedCs_(&pitCs_);
    
    std::map<Name, PitEntry>::iterator pitHit = pit_.find(prefix);
    
    // check for rightmost prefixes
    if (pitHit == pit_.end())
    {
        ndn::Name testPrefix(prefix);
        
        NdnRtcNamespace::trimPacketNumber(prefix, testPrefix);
        pitHit = pit_.find(testPrefix);
    }
    
    if (pitHit != pit_.end())
    {
        int64_t currentTime = NdnRtcUtils::millisecondTimestamp();
        
        shared_ptr<const Interest> pendingInterest = pitHit->second.interest_;
        
        metaInfo.interestNonce_ =
        NdnRtcUtils::blobToNonce(pendingInterest->getNonce());
        metaInfo.interestArrivalMs_ = pitHit->second.arrivalTimestamp_;
        metaInfo.generationDelayMs_ = (uint32_t)(currentTime - pitHit->second.arrivalTimestamp_);
        
        pit_.erase(pitHit);
        
        LogTraceC
        << "pit hit [" << prefix.toUri() << "] -> ["
        << pendingInterest->getName().toUri() << " (size "
        << pit_.size() << std::endl;
        
        return 1;
    }
    else
    {
        LogTraceC << "no pit entry " << prefix.toUri() << std::endl;
    }
    
    return 0;
}
