//
//  video-thread.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "video-thread.h"
#include "ndnlib.h"
#include "ndnrtc-utils.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc::new_api;
using namespace webrtc;

const double VideoThread::ParityRatio = 0.2;

VideoThread::VideoThread():
MediaThread(),
coder_(new VideoCoder())
{
    description_ = "vthread";
    coder_->registerCallback(this);
    coder_->setFrameConsumer(this);
}

//******************************************************************************
#pragma mark - overriden
int
VideoThread::init(const VideoThreadSettings& settings)
{
    int res = MediaThread::init(settings);
    
    if (RESULT_FAIL(res))
        return res;
    
    deltaFramesPrefix_ = Name(threadPrefix_);
    deltaFramesPrefix_.append(Name(NdnRtcNamespace::NameComponentStreamFramesDelta));
    keyFramesPrefix_ = Name(threadPrefix_);
    keyFramesPrefix_.append(Name(NdnRtcNamespace::NameComponentStreamFramesKey));
    
    if (RESULT_FAIL(coder_->init(settings_.getVideoParams()->coderParams_)))
        return notifyError(RESULT_ERR, "can't initialize video encoder");
    
    keyFrameNo_ = -1;
    deltaFrameNo_ = 0;
    
    LogInfoC << "initialized" << std::endl;
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - interfaces realization
void
VideoThread::onDeliverFrame(webrtc::I420VideoFrame &frame,
                               double unixTimeStamp)
{
    coder_->onDeliverFrame(frame, unixTimeStamp);
}

void
VideoThread::onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                        double captureTimestamp)
{
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    uint64_t publishingTime = NdnRtcUtils::microsecondTimestamp();
    
    // determine, whether we should publish under key or delta namespaces
    bool isKeyFrame = encodedImage._frameType == webrtc::kKeyFrame;
    
    if (isKeyFrame)
        keyFrameNo_++;
    
    Name framePrefix((isKeyFrame?keyFramesPrefix_:deltaFramesPrefix_));
    FrameNumber frameNo = (isKeyFrame)?keyFrameNo_:deltaFrameNo_;
    framePrefix.append(NdnRtcUtils::componentFromInt(frameNo));
    
    Name framePrefixData(framePrefix);
    framePrefixData.append(NdnRtcNamespace::NameComponentFrameSegmentData);
    
    PrefixMetaInfo prefixMeta = {0,0,0,0};
    prefixMeta.playbackNo_ = packetNo_;
    prefixMeta.pairedSequenceNo_ = (isKeyFrame)?deltaFrameNo_:keyFrameNo_;
    
    NdnFrameData frameData(encodedImage, segSizeNoHeader_);
    int nSegments = 0;
    
    int nSegmentsExpected = Segmentizer::getSegmentsNum(frameData.getLength(), segSizeNoHeader_);
    int nSegmentsParityExpected = (settings_.useFec_)?FrameParityData::getParitySegmentsNum(nSegmentsExpected, ParityRatio):0;
    
    prefixMeta.totalSegmentsNum_ = nSegmentsExpected;
    prefixMeta.paritySegmentsNum_ = nSegmentsParityExpected;
    
    if ((nSegments = publishPacket(frameData,
                                   framePrefixData,
                                   frameNo,
                                   prefixMeta,
                                   captureTimestamp)) > 0)
    {
        assert(nSegments == nSegmentsExpected);
        
        publishingTime = NdnRtcUtils::microsecondTimestamp() - publishingTime;
        
        LogDebugC
        << "publish\t" << packetNo_ << "\t"
        << deltaFrameNo_ << "\t"
        << keyFrameNo_ << "\t"
        << (isKeyFrame?"K":"D") << "\t"
        << publishingTime << "\t"
        << frameData.getLength() << "\t"
        << encodedImage.capture_time_ms_ << "\t"
        << NdnRtcUtils::currentFrequencyMeterValue(packetRateMeter_) << "\t"
        << nSegments << std::endl;
        
        if (settings_.useFec_)
            publishParityData(frameNo, frameData, nSegments, framePrefix,
                              prefixMeta);
        
        
        if (!isKeyFrame)
            deltaFrameNo_++;
        
        // increment packet number regardless of key/delta condition
        // in this case we can preserve that key frames can have numbers in
        // delta namespace as well
        packetNo_++;
    }
    else
    {
        notifyError(RESULT_ERR, "were not able to publish frame %d (KEY: %s)",
                    getPacketNo(), (isKeyFrame)?"YES":"NO");
    }
}

void
VideoThread::setLogger(ndnlog::new_api::Logger *logger)
{
    coder_->setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void
VideoThread::onInterest(const shared_ptr<const Name>& prefix,
                           const shared_ptr<const Interest>& interest,
                           ndn::Transport& transport)
{
    const Name& name = interest->getName();
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(name);
    bool isKeyNamespace = NdnRtcNamespace::isKeyFramePrefix(name);
    
    
    if ((NdnRtcNamespace::isValidInterestPrefix(name) && packetNo == -1) ||
        packetNo >= ((isKeyNamespace)?keyFrameNo_:deltaFrameNo_))
    {
        addToPit(interest);
    }
    
    LogTraceC << "incoming interest for " << interest->getName()
    << ((packetNo >= ((isKeyNamespace)?keyFrameNo_:deltaFrameNo_) || packetNo == -1)?" (new)":" (old)") << std::endl;
}

int
VideoThread::publishParityData(PacketNumber frameNo,
                                  const PacketData& packetData,
                                  unsigned int nSegments,
                                  const Name& framePrefix,
                                  const PrefixMetaInfo& prefixMeta)
{
    int nSegmentsP = -1;
    FrameParityData frameParityData;
    
    if (RESULT_GOOD(frameParityData.initFromPacketData(packetData, ParityRatio,
                                                       nSegments, segSizeNoHeader_)))
    {
        //Prefix
        Name framePrefixParity(framePrefix);
        NdnRtcNamespace::appendStringComponent(framePrefixParity,
                                               NdnRtcNamespace::NameComponentFrameSegmentParity);
        //Publish Packet of Parity
        if ((nSegmentsP = publishPacket(frameParityData,
                                        framePrefixParity,
                                        frameNo,
                                        prefixMeta,
                                        NdnRtcUtils::unixTimestamp())) > 0)
        {
            LogDebugC
            << "publish parity\t" << packetNo_ << "\t"
            << deltaFrameNo_ << "\t"
            << keyFrameNo_ << "\t"
            << ((frameNo == keyFrameNo_)?"K":"D") << "\t"
            << frameParityData.getLength() << "\t"
            << nSegmentsP << std::endl;
        }
        else
        {
            notifyError(RESULT_ERR, "were not able to publish parity data %d",
                        getPacketNo());
        }
    }
    else
    {
        LogErrorC << "FEC Encoding Failure" << std::endl;
    }
    
    
    return nSegmentsP;
}
