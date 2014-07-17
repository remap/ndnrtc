//
//  video-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "video-sender.h"
#include "ndnlib.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace webrtc;

const double NdnVideoSender::ParityRatio = 0.2;

NdnVideoSender::NdnVideoSender(const ParamsStruct& params,
                               const CodecParams& codecParams):
MediaSender(params),
codecParams_(codecParams),
coder_(new NdnVideoCoder(codecParams))
{
    description_ = "vsender";
    coder_->setObserver(this);
    coder_->setFrameConsumer(this);
}

//******************************************************************************
#pragma mark - overriden
int NdnVideoSender::init(const shared_ptr<FaceProcessor>& faceProcessor,
                         const shared_ptr<KeyChain>& ndnKeyChain)
{
    shared_ptr<string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_, codecParams_.idx);
    int res = MediaSender::init(faceProcessor, ndnKeyChain, packetPrefix);
    
    if (RESULT_FAIL(res))
        return res;
    
    if (RESULT_FAIL(coder_->init()))
        return notifyError(RESULT_ERR, "can't initialize video encoder");
    
    ptr_lib::shared_ptr<string>
    keyPrefixString = NdnRtcNamespace::getStreamFramePrefix(params_, codecParams_.idx, true);
    
    keyFramesPrefix_.reset(new Name(*keyPrefixString));
    keyFrameNo_ = -1;
    deltaFrameNo_ = 0;
    
    registerPrefix(keyFramesPrefix_);
    
    LogInfoC << "initialized" << endl;
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - interfaces realization
void NdnVideoSender::onDeliverFrame(webrtc::I420VideoFrame &frame,
                                    double unixTimeStamp)
{
    coder_->onDeliverFrame(frame, unixTimeStamp);
}

void NdnVideoSender::onEncodedFrameDelivered(const webrtc::EncodedImage &encodedImage,
                                             double captureTimestamp)
{
    // update packet rate meter
    NdnRtcUtils::frequencyMeterTick(packetRateMeter_);
    uint64_t publishingTime = NdnRtcUtils::microsecondTimestamp();
    
    // determine, whether we should publish under key or delta namespaces
    bool isKeyFrame = encodedImage._frameType == webrtc::kKeyFrame;
    
    if (isKeyFrame)
        keyFrameNo_++;
    
    shared_ptr<Name> tmpPrefix = (isKeyFrame)?keyFramesPrefix_:packetPrefix_;
    shared_ptr<Name> framePrefix(new Name(*tmpPrefix));
    
    FrameNumber frameNo = (isKeyFrame)?keyFrameNo_:deltaFrameNo_;
    
    framePrefix->append(NdnRtcUtils::componentFromInt(frameNo));
    
    shared_ptr<Name> framePrefixData(new Name(*framePrefix));
    NdnRtcNamespace::appendStringComponent(framePrefixData,
                                           NdnRtcNamespace::NameComponentFrameSegmentData);
    
    PrefixMetaInfo prefixMeta = {0,0,0,0};
    prefixMeta.playbackNo_ = packetNo_;
    prefixMeta.pairedSequenceNo_ = (isKeyFrame)?deltaFrameNo_:keyFrameNo_;
    
    NdnFrameData frameData(encodedImage, segmentSize_);
    int nSegments = 0;
    
    int nSegmentsExpected = Segmentizer::getSegmentsNum(frameData.getLength(), segmentSize_);
    int nSegmentsParityExpected = (params_.useFec)?FrameParityData::getParitySegmentsNum(nSegmentsExpected, ParityRatio):0;
    
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
//        INFO("PUBLISHED: \t%d \t%d \t%ld \t%d \t%ld \t%.2f",
//             getFrameNo(),
//             isKeyFrame,
//             publishingTime,
//             frameData.getLength(),
//             encodedImage.capture_time_ms_,
//             getCurrentPacketRate());
//        webrtc::EncodedImage *eimg = (webrtc::EncodedImage*)(&encodedImage);
//        ewriter.writeFrame(*eimg, metadata);
        
        LogDebugC
        << "publish\t" << packetNo_ << "\t"
        << deltaFrameNo_ << "\t"
        << keyFrameNo_ << "\t"
        << (isKeyFrame?"K":"D") << "\t"
        << publishingTime << "\t"
        << frameData.getLength() << "\t"
        << encodedImage.capture_time_ms_ << "\t"
        << getCurrentPacketRate() << "\t"
        << nSegments << endl;
        
        if (params_.useFec)
        {
            int nSegmentsParity = publishParityData(frameNo, frameData,
                                                    nSegments, framePrefix,
                                                    prefixMeta);
//            assert(nSegmentsParity == nSegmentsParityExpected);
        }
        
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
                    getFrameNo(), (isKeyFrame)?"YES":"NO");
    }
}

void NdnVideoSender::setLogger(ndnlog::new_api::Logger *logger)
{
    coder_->setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void NdnVideoSender::onInterest(const shared_ptr<const Name>& prefix,
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
    << ((packetNo >= ((isKeyNamespace)?keyFrameNo_:deltaFrameNo_) || packetNo == -1)?" (new)":" (old)") << endl;
}

int
NdnVideoSender::publishParityData(PacketNumber frameNo,
                                  const PacketData &packetData,
                                  unsigned int nSegments,
                                  const shared_ptr<Name>& framePrefix,
                                  const PrefixMetaInfo& prefixMeta)
{
    int nSegmentsP = -1;
    FrameParityData frameParityData;
    
    if (RESULT_GOOD(frameParityData.initFromPacketData(packetData, ParityRatio,
                                                       nSegments, segmentSize_)))
    {
        //Prefix
        shared_ptr<Name> framePrefixParity(new Name(*framePrefix));
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
            << nSegmentsP << endl;
        }
        else
        {
            notifyError(RESULT_ERR, "were not able to publish parity data %d",
                        getFrameNo());
        }
    }
    else
    {
        LogErrorC << "FEC Encoding Failure" << endl;
    }
    
    
    return nSegmentsP;
}
