//
//  media-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <ndn-cpp/security/key-chain.hpp>

#include "media-thread.h"
#include "ndnrtc-namespace.h"
#include "ndnrtc-utils.h"
#include "ndnrtc-debug.h"
#include "face-wrapper.h"
#include "segmentizer.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc::new_api;
using namespace webrtc;

//******************************************************************************
#pragma mark - construction/destruction
MediaThread::MediaThread():
NdnRtcComponent(),
packetNo_(0)
{
    packetRateMeter_ = NdnRtcUtils::setupFrequencyMeter(4);
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(5);
}

MediaThread::~MediaThread()
{
    delete settings_;
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
    publishingTimestampMs_ = 0;
    threadPrefix_ =  *NdnRtcNamespace::buildPath(false,
                                                 &settings.streamPrefix_,
                                                 &settings.threadParams_->threadName_, 0);
    faceProcessor_ = settings.faceProcessor_;
    memCache_ = settings.memoryCache_;
    
    try {
        registerPrefix(Name(threadPrefix_));
    }
    catch (std::exception &e)
    {
        return notifyError(RESULT_ERR,
                           "got error from ndn library while registering prefix: %s",
                           e.what());
    }
    
    segSizeNoHeader_ = settings.segmentSize_ - SegmentData::getHeaderSize();
    
    return RESULT_OK;
}

void MediaThread::stop()
{
    if (memCache_.get())
        memCache_->unregisterAll();
    else
        faceProcessor_->getFaceWrapper()->unregisterPrefix(registeredPrefixId_);
}

//******************************************************************************
#pragma mark - private
int MediaThread::publishPacket(PacketData &packetData,
                               const Name& packetPrefix,
                               PacketNumber packetNo,
                               PrefixMetaInfo prefixMeta,
                               double captureTimestamp)
{
    if (!faceProcessor_->getTransport()->getIsConnected())
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
        
        prefixMeta.crcValue_ = packetData.getCrcValue();
        
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
            ndnData.getMetaInfo().setFreshnessPeriod(settings_->dataFreshnessMs_);
            ndnData.setContent(segmentData.getData(), segmentData.getLength());
            
            settings_->keyChain_->sign(ndnData, settings_->certificateName_);
            
            if (memCache_.get() && !pitHit)
            {
                // according to http://named-data.net/doc/ndn-ccl-api/memory-content-cache.html#memorycontentcache-registerprefix-method
                // adding content should be synchronized with the processEvents
                // call; this must be done in calling routines
                memCache_->add(ndnData);
                
                LogTraceC
                << "added to cache " << segmentName << " "
                << ndnData.getContent().size() << " bytes" << std::endl;
            }
            else
            {
                SignedBlob encodedData = ndnData.wireEncode();
                faceProcessor_->getTransport()->send(*encodedData);
                
                LogTraceC
                << "sent " << segmentName << " "
                << ndnData.getContent().size() << " bytes" << std::endl;
            }
            
#if 0   // enable this if you want measuring outgoing bitrate w/o ndn overhead
            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                               ndnData.getContent().size());
#else   // enable this if you want measuring outgoing bitrate with ndn overhead
            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                               ndnData.getDefaultWireEncoding().size());
#endif
        } // for
        
        cleanPitForFrame(prefix);
    }
    catch (std::exception &e)
    {
        return notifyError(RESULT_ERR,
                           "got error from ndn library while sending data: %s",
                           e.what());
    }
    
    
    int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
    
    if (publishingTimestampMs_)
    {
        int64_t delay = timestamp-publishingTimestampMs_;
        
        ((delay > FRAME_DELAY_DEADLINE) ? LogWarnC : LogTraceC)
        << "frame publishing delay " << delay
        << " (" << packetPrefix << ")" << std::endl;
    }
    
    publishingTimestampMs_ = timestamp;
    
    return segments.size();
}

void MediaThread::registerPrefix(const Name& prefix)
{
    if (memCache_.get())
    {
        // MemoryContentCache does not support providing registered prefixes IDs
        // registeredPrefixId_ =
        memCache_->registerPrefix(prefix,
                                  bind(&MediaThread::onRegisterFailed,
                                       this, _1),
                                  bind(&MediaThread::onInterest,
                                       this, _1, _2, _3, _4, _5));
    }
    else
    {
//        registeredPrefixId_ = faceProcessor_->getFaceWrapper()->registerPrefix(prefix,
//                                                                             bind(&MediaThread::onInterest,
//                                                                                  this, _1, _2, _3, _4, _5),
//                                                                             bind(&MediaThread::onRegisterFailed,
//                                                                                  this, _1));
//        if (registeredPrefixId_ != 0)
//            LogTraceC << "registered prefix " << prefix << std::endl;
    }
}

void MediaThread::onInterest(const shared_ptr<const Name>& prefix,
                             const shared_ptr<const Interest>& interest,
                             ndn::Face& face,
                             uint64_t ts,
                             const shared_ptr<const InterestFilter>& filter)
{
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(interest->getName());

    LogTraceC << "incoming interest for " << interest->getName()
    << ((packetNo >= packetNo_ || packetNo == -1)?" (new)":" (old)") << ", current #" << packetNo_ << std::endl;
    
    if (packetNo >= packetNo_)
    {
        addToPit(interest);
    }
}

void MediaThread::onRegisterFailed(const shared_ptr<const Name>& prefix)
{
//    if (hasCallback())
//        getCallback()->onMediaThreadRegistrationFailed(settings_->threadParams_->threadName_);
    
    notifyError(-1, "registration on %s has failed", prefix->toUri().c_str());
}

void MediaThread::addToPit(const shared_ptr<const ndn::Interest> &interest)
{
    const Name& name = interest->getName();
    PitEntry pitEntry;
    
    pitEntry.arrivalTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    pitEntry.interest_ = interest;
    
    {
        lock_guard<mutex> scopedLock(pitMutex_);
        
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
    lock_guard<mutex> scopedLock(pitMutex_);
    
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
        << pit_.size()  << ")]" << std::endl;
        
        return 1;
    }
    else
    {
        LogTraceC << "no pit entry " << prefix.toUri() << std::endl;
    }
    
    return 0;
}

int MediaThread::cleanPitForFrame(const Name framePrefix)
{
    lock_guard<mutex> scopedLock(pitMutex_);
    std::vector<Name> keysToDelete;
    
    for (auto it:pit_)
        if (NdnRtcNamespace::isPrefix(it.first, framePrefix))
            keysToDelete.push_back(it.first);
    
    for (auto key : keysToDelete)
    {
        LogTraceC << "purge old PIT entry " << key;
        pit_.erase(key);
    }
    
    return keysToDelete.size();
}
