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
#include "ndnrtc-utils.h"
#include "face-wrapper.h"
#include "ndnrtc-namespace.h"
#include "segmentizer.h"

using namespace boost;
using namespace ndnlog;
using namespace ndnrtc::new_api;
using namespace webrtc;



//******************************************************************************
const double VideoThread::ParityRatio = 0.2;

VideoThread::VideoThread():
MediaThread(),
coder_(new VideoCoder()),
deltaSegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, 0)),
keySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, 0)),
deltaParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, 0)),
keyParitySegnumEstimatorId_(NdnRtcUtils::setupMeanEstimator(0, 0)),
callback_(nullptr)
{
    description_ = "vthread";
    coder_->registerCallback(this);
    coder_->setFrameConsumer(this);
    startMyThread();
}

VideoThread::~VideoThread()
{
}

//******************************************************************************
#pragma mark - overriden
int
VideoThread::init(const VideoThreadSettings& settings)
{
    settings_ = new VideoThreadSettings();
    *((VideoThreadSettings*)settings_) = settings;
    
    int res = MediaThread::init(settings);
    
    if (RESULT_FAIL(res))
        return res;
    
    callback_ = settings.threadCallback_;
    deltaFramesPrefix_ = Name(threadPrefix_);
    deltaFramesPrefix_.append(Name(NameComponents::NameComponentStreamFramesDelta));
    keyFramesPrefix_ = Name(threadPrefix_);
    keyFramesPrefix_.append(Name(NameComponents::NameComponentStreamFramesKey));
    
    if (RESULT_FAIL(coder_->init(getSettings().getVideoParams()->coderParams_)))
        return notifyError(RESULT_ERR, "can't initialize video encoder");
    
    keyFrameNo_ = -1;
    deltaFrameNo_ = 0;
    
    LogInfoC << "initialized" << std::endl;
    
    return RESULT_OK;
}

void
VideoThread::stop()
{
    lock_guard<mutex> processingLock(frameProcessingMutex_);
    MediaThread::stop();
    stopMyThread();
}

void
VideoThread::getStatistics(VideoThreadStatistics& statistics)
{
    MediaThread::getStatistics(statistics);
    coder_->getStatistics(statistics.coderStatistics_);
    statistics.nKeyFramesPublished_ = keyFrameNo_;
    statistics.nDeltaFramesPublished_ = deltaFrameNo_;
    statistics.deltaAvgSegNum_ = NdnRtcUtils::currentMeanEstimation(deltaSegnumEstimatorId_);
    statistics.keyAvgSegNum_ = NdnRtcUtils::currentMeanEstimation(keySegnumEstimatorId_);
    statistics.deltaAvgParitySegNum_ = NdnRtcUtils::currentMeanEstimation(deltaParitySegnumEstimatorId_);
    statistics.keyAvgParitySegNum_ = NdnRtcUtils::currentMeanEstimation(keyParitySegnumEstimatorId_);
}


//******************************************************************************
#pragma mark - interfaces realization
void
VideoThread::onDeliverFrame(WebRtcVideoFrame &frame,
                               double unixTimeStamp)
{
    if (frameProcessingMutex_.try_lock())
    {
        deliveredFrame_.CopyFrame(frame);
        dispatchOnMyThread([this, unixTimeStamp](){
            lock_guard<mutex> processingLock(frameProcessingMutex_);
            encodeFinished_ = false;
            coder_->onDeliverFrame(deliveredFrame_, unixTimeStamp);
            
            if (!encodeFinished_ && callback_)
            {
                // we must continue with packet counter, in order to be
                // synchronized with other threads
                packetNo_++;
                callback_->onFrameDropped(settings_->threadParams_->threadName_);
            }
        });
        frameProcessingMutex_.unlock();
    }
    else
        LogWarnC << "delivered new raw frame while previous"
        " is still being processed" << std::endl;

}

void
VideoThread::onEncodingStarted()
{
    encodingTimestampMs_ = NdnRtcUtils::millisecondTimestamp();
}

void VideoThread::onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                          double captureTimestamp,
                                          bool completeFrame)
{
    encodeFinished_ = true;
    if (callback_) {
        FrameNumber frameNo = (encodedImage._frameType == webrtc::kKeyFrame)? (keyFrameNo_+1):deltaFrameNo_;
        callback_->onFrameEncoded(settings_->threadParams_->threadName_, frameNo,
                                  encodedImage._frameType == webrtc::kKeyFrame);
    }
    
    // as we're using ThreadsafeFace we need to synchronize with it
    // the only way to sync is to execute code which accesses Face on
    // the same thread that ThreadsafeFace is running.
    // thus, we'll dispatch further work onto main background NDN-RTC
    // thread synchronously
    NdnRtcUtils::performOnBackgroundThread([this, &encodedImage, captureTimestamp, completeFrame](){
        publishFrameData(encodedImage, captureTimestamp, completeFrame);
    });
}

void
VideoThread::publishFrameData(const webrtc::EncodedImage &encodedImage,
                              double captureTimestamp,
                              bool completeFrame)
{
    int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
    
    // in order to avoid situations when interest arrives simultaneously
    // with the data being added to the PIT/cache, we synchronize with
    // face on this level
    faceProcessor_->getFaceWrapper()->synchronizeStart();
    
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    
    LogDebugC
    << "producing rate " << NdnRtcUtils::currentFrequencyMeterValue(packetRateMeter_)
    << std::endl;
    
    // determine, whether we should publish under key or delta namespaces
    bool isKeyFrame = encodedImage._frameType == webrtc::kKeyFrame;
    
    if (isKeyFrame)
    {
        keyFrameNo_++;
        gopCount_ = 0;
    }
    else
        gopCount_++;
    
    int64_t encodingDelay = timestamp - encodingTimestampMs_;
    
    (encodingDelay > FRAME_DELAY_DEADLINE ? LogWarnC : LogTraceC)
    << "encoding delay " << encodingDelay
    << " type " << (isKeyFrame?" key":" delta")
    << ". delta " << deltaFrameNo_
    << " key " << keyFrameNo_
    << " abs " << packetNo_
    << " gop count " << gopCount_
    << std::endl;
        
    Name framePrefix((isKeyFrame?keyFramesPrefix_:deltaFramesPrefix_));
    FrameNumber frameNo = (isKeyFrame)?keyFrameNo_:deltaFrameNo_;
    framePrefix.append(NdnRtcUtils::componentFromInt(frameNo));
    
    Name framePrefixData(framePrefix);
    framePrefixData.append(NameComponents::NameComponentFrameSegmentData);
    
    PrefixMetaInfo prefixMeta = PrefixMetaInfo::ZeroMetaInfo;
    prefixMeta.playbackNo_ = packetNo_;
    prefixMeta.pairedSequenceNo_ = (isKeyFrame)?deltaFrameNo_:keyFrameNo_;
    
    NdnFrameData frameData(encodedImage, segSizeNoHeader_);
    int nSegments = 0;
    
    int nSegmentsExpected = Segmentizer::getSegmentsNum(frameData.getLength(), segSizeNoHeader_);
    int nSegmentsParityExpected = (getSettings().useFec_)?FrameParityData::getParitySegmentsNum(nSegmentsExpected, ParityRatio):0;
    
    prefixMeta.totalSegmentsNum_ = nSegmentsExpected;
    prefixMeta.paritySegmentsNum_ = nSegmentsParityExpected;
    prefixMeta.syncList_ = callback_->getFrameSyncList(isKeyFrame);
    
    if ((nSegments = publishPacket(frameData,
                                   framePrefixData,
                                   frameNo,
                                   prefixMeta,
                                   captureTimestamp)) > 0)
    {
        assert(nSegments == nSegmentsExpected);
        
        LogDebugC
        << "publish\t" << packetNo_ << "\t"
        << deltaFrameNo_ << "\t"
        << keyFrameNo_ << "\t"
        << (isKeyFrame?"K":"D") << "\t"
        << (NdnRtcUtils::millisecondTimestamp()-timestamp) << "\t"
        << frameData.getLength() << "\t"
        << encodedImage.capture_time_ms_ << "\t"
        << NdnRtcUtils::currentFrequencyMeterValue(packetRateMeter_) << "\t"
        << nSegments << std::endl;
        
        int nSegmentsParity = -1;
        if (getSettings().useFec_)
            nSegmentsParity = publishParityData(frameNo, frameData, nSegments, framePrefix,
                                                prefixMeta);
        
        
        if (!isKeyFrame)
            deltaFrameNo_++;
        
        if (isKeyFrame)
            NdnRtcUtils::meanEstimatorNewValue(keySegnumEstimatorId_, nSegments);
        else
            NdnRtcUtils::meanEstimatorNewValue(deltaSegnumEstimatorId_, nSegments);
        
        if (getSettings().useFec_ && nSegmentsParity > 0)
        {
            if (isKeyFrame)
                NdnRtcUtils::meanEstimatorNewValue(keyParitySegnumEstimatorId_, nSegmentsParity);
            else
                NdnRtcUtils::meanEstimatorNewValue(deltaParitySegnumEstimatorId_, nSegmentsParity);
        }
        
        // increment packet number regardless of key/delta condition
        // in this case we can preserve that key frames can have numbers in
        // delta namespace as well
        packetNo_++;
    }
    else
    {
        LogErrorC << "was not able to publish frame " << getPacketNo()
        << " KEY: " << ((isKeyFrame)?"YES":"NO") << std::endl;
    }

    faceProcessor_->getFaceWrapper()->synchronizeStop();
}

void
VideoThread::onFrameDropped()
{
    LogWarnC << "frame dropped by encoder."
    << " delta " << deltaFrameNo_
    << " key " << keyFrameNo_
    << " abs " << packetNo_
    << " gop count " << gopCount_
    << std::endl;
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
                        ndn::Face& face,
                        uint64_t ts,
                        const shared_ptr<const InterestFilter>& filter)
{
    const Name& name = interest->getName();
    PacketNumber packetNo = NdnRtcNamespace::getPacketNumber(name);
    bool isKeyNamespace = NdnRtcNamespace::isKeyFramePrefix(name);
    
    LogTraceC << "incoming interest for " << interest->getName()
    << ((packetNo >= ((isKeyNamespace)?keyFrameNo_:deltaFrameNo_) || packetNo == -1)?" (new)":" (old)") << std::endl;
    
    if ((NdnRtcNamespace::isValidInterestPrefix(name) && packetNo == -1) ||
        packetNo >= ((isKeyNamespace)?keyFrameNo_:deltaFrameNo_))
    {
        addToPit(interest);
    }
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
                                               NameComponents::NameComponentFrameSegmentParity);
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
