//
//  media-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "media-sender.h"
#include "ndnrtc-namespace.h"
#include "ndnrtc-utils.h"
#include "ndnrtc-debug.h"

using namespace std;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc;
using namespace webrtc;

#define RECORD 0
#if RECORD
#include "ndnrtc-testing.h"
using namespace ndnrtc::testing;

static EncodedFrameWriter frameWriter("encoded.nrtc");
static unsigned char* frameData = (unsigned char*)malloc(640*480);
static int dataLength = 0;

#endif

//******************************************************************************
#pragma mark - construction/destruction
MediaSender::MediaSender(const ParamsStruct &params) :
NdnRtcObject(params),
packetNo_(0),
pitCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection())
{
    packetRateMeter_ = NdnRtcUtils::setupFrequencyMeter(4);
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
}

MediaSender::~MediaSender()
{
    stop();
    
    NdnRtcUtils::releaseFrequencyMeter(packetRateMeter_);
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
}

//******************************************************************************
//******************************************************************************
#pragma mark - static


//******************************************************************************
#pragma mark - public
int MediaSender::init(const shared_ptr<FaceProcessor>& faceProcessor,
                      const shared_ptr<KeyChain>& ndnKeyChain)
{
    faceProcessor_ = faceProcessor;
    ndnKeyChain_ = ndnKeyChain;
    
#ifndef DEFAULT_KEYCHAIN
    shared_ptr<string> userPrefix = NdnRtcNamespace::getUserPrefix(params_);
    certificateName_ =  NdnRtcNamespace::certificateNameForUser(*userPrefix);
#else
    certificateName_ = shared_ptr<Name>(new Name(ndnKeyChain_->getDefaultCertificateName()));
#endif
    
    if (params_.useCache)
        memCache_.reset(new MemoryContentCache(faceProcessor_->getFaceWrapper()->getFace().get()));
    
    shared_ptr<string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    if (!packetPrefix.get())
        notifyError(-1, "bad frame prefix");
    
    packetPrefix_.reset(new Name(packetPrefix->c_str()));
    
    registerPrefix(packetPrefix_);
    
    segmentSize_ = params_.segmentSize - SegmentData::getHeaderSize();
    freshnessInterval_ = params_.freshness;
    packetNo_ = 0;
    
    return RESULT_OK;
}

void MediaSender::stop()
{
#if RECORD
    frameWriter.synchronize();
#endif
}

//******************************************************************************
#pragma mark - private
int MediaSender::publishPacket(const PacketData &packetData,
                               shared_ptr<Name> packetPrefix,
                               PacketNumber packetNo,
                               PrefixMetaInfo prefixMeta)
{
    if (!faceProcessor_->getTransport()->getIsConnected())
        return notifyError(-1, "transport is not connected");
    
    Name prefix = *packetPrefix;
    Segmentizer::SegmentList segments;
    
    if (RESULT_FAIL(Segmentizer::segmentize(packetData, segments, segmentSize_)))
        return notifyError(-1, "packet segmentation failed");
    
    try {
#if RECORD
        dataLength = 0;
        memset(frameData, 0, 640*480);
#endif
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
            ndnData.getMetaInfo().setFreshnessPeriod(params_.freshness*1000);
            ndnData.setContent(segmentData.getData(), segmentData.getLength());
            
            ndnKeyChain_->sign(ndnData, *certificateName_);
            
            if (params_.useCache && !pitHit)
            {
                // according to http://named-data.net/doc/ndn-ccl-api/memory-content-cache.html#memorycontentcache-registerprefix-method
                // adding content should be synchronized with the processEvents
                // call
                faceProcessor_->getFaceWrapper()->synchronizeStart();
                memCache_->add(ndnData);
                faceProcessor_->getFaceWrapper()->synchronizeStop();
                
                LogTraceC
                << "added to cache " << segmentName << " "
                << ndnData.getContent().size() << " bytes" << endl;
            }
            else
            {
                SignedBlob encodedData = ndnData.wireEncode();
                faceProcessor_->getTransport()->send(*encodedData);
                
                LogTraceC
                << "published " << segmentName << " "
                << ndnData.getContent().size() << " bytes" << endl;
            }
            
            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                               ndnData.getContent().size());
            
#if RECORD
            {
                SegmentData segData;
                SegmentData::segmentDataFromRaw(ndnData.getContent().size(),
                                                ndnData.getContent().buf(),
                                                segData);
                SegmentNumber segNo = NdnRtcNamespace::getSegmentNumber(ndnData.getName());
                memcpy(frameData+segNo*segmentSize_,
                       segData.getSegmentData(),
                       segData.getSegmentDataLength());
                dataLength += segData.getSegmentDataLength();
            }
#endif
        }
        
#if RECORD
        PacketData *frame;
        PacketData::packetFromRaw(dataLength,
                                  frameData,
                                  &frame);
        assert(frame);
        
        webrtc::EncodedImage img;
        ((NdnFrameData*)frame)->getFrame(img);
        PacketData::PacketMetadata meta = ((NdnFrameData*)frame)->getMetadata();
        frameWriter.writeFrame(img, meta);
#endif
    }
    catch (std::exception &e)
    {
        return notifyError(-1,
                           "got error from ndn library while sending data: %s",
                           e.what());
    }
    
    return segments.size();
}

void MediaSender::registerPrefix(const shared_ptr<Name>& prefix)
{
    // this is a key chain workaround:
    // media sender is given a key chain upon initialization (@see init method)
    // this key chain is used for signing individual data segments. this means,
    // sign method will be invoked >100 times per second. to decrease costs of
    // retrieving private key while signing, this key chain should be created
    // using MemoryIdentityStorage. however, the same key chain can not be
    // used for registering a prefix - in this case, NFD will not recognize the
    // key, created in app memory. this is why, we are using default key chain
    // here which talks to OS key chain and provides default certificate for
    // signing control interest - in this case, NFD can recognize this
    // certificate
#ifndef DEFAULT_KEYCHAIN
    KeyChain keyChain;
    faceProcessor_->getFaceWrapper()->setCommandSigningInfo(keyChain,
                                                            keyChain.getDefaultCertificateName());
#else
    faceProcessor_->getFaceWrapper()->setCommandSigningInfo(*ndnKeyChain_,
                                                            ndnKeyChain_->getDefaultCertificateName());
#endif
    
    if (params_.useCache)
    {
        memCache_->registerPrefix(*prefix,
                                  bind(&MediaSender::onRegisterFailed,
                                       this, _1),
                                  bind(&MediaSender::onInterest,
                                       this, _1, _2, _3));
    }
    else
    {
        uint64_t prefixId = faceProcessor_->getFaceWrapper()->registerPrefix(*prefix,
                                                                             bind(&MediaSender::onInterest,
                                                                                  this, _1, _2, _3),
                                                                             bind(&MediaSender::onRegisterFailed,
                                                                                  this, _1));
        if (prefixId != 0)
            LogTraceC << "registered prefix " << *prefix << endl;
    }
}

void MediaSender::onInterest(const shared_ptr<const Name>& prefix,
                             const shared_ptr<const Interest>& interest,
                             ndn::Transport& transport)
{
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(interest->getName());
    
    if (packetNo >= packetNo_)
    {
        addToPit(interest);
    }
    
    LogTraceC << "incoming interest for " << interest->getName()
    << ((packetNo >= packetNo_ || packetNo == -1)?" (new)":" (old)") << endl;
}

void MediaSender::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    notifyError(-1, "registration on %s has failed", prefix->toUri().c_str());
}

void MediaSender::addToPit(const shared_ptr<const ndn::Interest> &interest)
{
    const Name& name = interest->getName();
    PitEntry pitEntry;
    
    pitEntry.arrivalTimestamp_ = NdnRtcUtils::millisecondTimestamp();
    pitEntry.interest_ = interest;
    
    {
        webrtc::CriticalSectionScoped scopedCs_(&pitCs_);
        
        if (pit_.find(name) != pit_.end())
        {
            LogTraceC << "pit exists " << name << endl;
        }
        else
        {
            pit_[name] = pitEntry;
            LogTraceC << "new pit entry " << name << " " << pit_.size() << endl;
        }
    }
}

int MediaSender::lookupPrefixInPit(const ndn::Name &prefix,
                                   SegmentData::SegmentMetaInfo &metaInfo)
{
    webrtc::CriticalSectionScoped scopedCs_(&pitCs_);
    
    map<Name, PitEntry>::iterator pitHit = pit_.find(prefix);
    
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
        << pendingInterest->getName().toUri() << " (size " << pit_.size() << endl;
        
        return 1;
    }
    else
    {
        LogTraceC << "no pit entry " << prefix.toUri() << endl;
    }
    
    return 0;
}
