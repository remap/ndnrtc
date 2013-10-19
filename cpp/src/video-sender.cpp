//
//  video-sender.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

//#define NDN_LOGGING
//#define NDN_INFO
//#define NDN_WARN
//#define NDN_ERROR

#define NDN_DETAILED
#define NDN_TRACE
#define NDN_DEBUG

#include "video-sender.h"
#include "ndnlib.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnrtc;
using namespace webrtc;

//********************************************************************************
//********************************************************************************
//********************************************************************************
#pragma mark - public
string VideoSenderParams::getStreamPrefix() const
{
    
    char *hub = (char*)malloc(256);
    char *user = (char*)malloc(256);
    char *stream = (char*)malloc(256);

    memset(hub,0,256);
    memset(user,0,256);
    memset(stream,0,256);

    getHub(&hub);
    getProducerId(&user);
    getStreamName(&stream);
    
    shared_ptr<string> prefix = NdnRtcNamespace::getStreamPath(hub, user, stream);
    
//    prefix->copy(*streamPrefix, prefix->length(), 0);
    
    free(hub);
    free(user);
    free(stream);
    
    return *prefix;
}

string VideoSenderParams::getStreamKeyPrefix() const
{
    string prefix  = getStreamPrefix();
    shared_ptr<string> accessPrefix = NdnRtcNamespace::buildPath(false, &prefix, &NdnRtcNamespace::NdnRtcNamespaceComponentStreamKey, NULL);
    
    return *accessPrefix;
}

string VideoSenderParams::getStreamFramePrefix() const
{
    string prefix  = getStreamPrefix();
    
#warning temporarily set stream thread
    string streamThread = "vp8";
    shared_ptr<string> framesPrefix = NdnRtcNamespace::buildPath(false, &prefix, &streamThread, &NdnRtcNamespace::NdnRtcNamespaceComponentStreamFrames, NULL);
    
    return *framesPrefix;
}

string VideoSenderParams::getUserPrefix() const
{
    const std::string hub = getParamAsString(ParamNameNdnHub);
    const std::string producerId = getParamAsString(ParamNameProducerId);
    
    shared_ptr<string> userPrefix = NdnRtcNamespace::getProducerPrefix(hub, producerId);
    return *userPrefix;
}

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction
NdnFrameData::NdnFrameData(EncodedImage &frame)
{
    unsigned int headerSize_ = sizeof(FrameDataHeader);
    
    length_ = frame._length+headerSize_;
    data_ = (unsigned char*)malloc(length_);
    
    // copy frame data with offset of header
    memcpy(data_+headerSize_, frame._buffer, frame._length);
    
    // setup header
    ((FrameDataHeader*)(&data_[0]))->headerMarker_ = NDNRTC_FRAMEHDR_MRKR;
    ((FrameDataHeader*)(&data_[0]))->encodedWidth_ = frame._encodedWidth;
    ((FrameDataHeader*)(&data_[0]))->encodedHeight_ = frame._encodedHeight;
    ((FrameDataHeader*)(&data_[0]))->timeStamp_ = frame._timeStamp;
    ((FrameDataHeader*)(&data_[0]))->capture_time_ms_ = frame.capture_time_ms_;
    ((FrameDataHeader*)(&data_[0]))->frameType_ = frame._frameType;
    ((FrameDataHeader*)(&data_[0]))->completeFrame_ = frame._completeFrame;
    ((FrameDataHeader*)(&data_[0]))->bodyMarker_ = NDNRTC_FRAMEBODY_MRKR;
}
NdnFrameData::~NdnFrameData()
{
    free(data_);
}

//********************************************************************************
#pragma mark - public
int NdnFrameData::unpackFrame(unsigned int length_, const unsigned char *data, webrtc::EncodedImage **frame)
{
    unsigned int headerSize_ = sizeof(FrameDataHeader);    
    FrameDataHeader header = *((FrameDataHeader*)(&data[0]));
    
    // check markers
    if (header.headerMarker_ != NDNRTC_FRAMEHDR_MRKR &&
        header.bodyMarker_ != NDNRTC_FRAMEBODY_MRKR)
        return -1;
    
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, header.encodedWidth_, header.encodedHeight_);
    
    *frame = new webrtc::EncodedImage(const_cast<uint8_t*>(&data[headerSize_]), length_-headerSize_, size);
    (*frame)->_encodedWidth = header.encodedWidth_;
    (*frame)->_encodedHeight = header.encodedHeight_;
    (*frame)->_timeStamp = header.timeStamp_;
    (*frame)->capture_time_ms_ = header.capture_time_ms_;
    (*frame)->_frameType = header.frameType_;
    (*frame)->_completeFrame = header.completeFrame_;

    return 0;
}

//********************************************************************************
//********************************************************************************
#pragma mark - construction/destruction

//********************************************************************************
#pragma mark - public
//void NdnVideoSender::registerConference(ndnrtc::VideoSenderParams &params)
//{
//    params_.resetParams(params);
//    TRACE("register conference");
//}

//void NdnVideoSender::fetchParticipantsList()
//{
//    TRACE("fetch participants");
//}


//********************************************************************************
#pragma mark - intefaces realization
void NdnVideoSender::onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
{
    if (!ndnTransport_->getIsConnected())
    {
        notifyError(-1, "can't send packet - no connection");
        return;
    }
    // send frame over NDN
    TRACE("sending frame #%010lld", frameNo_);
    
    // 0. copy frame into transport data object
    NdnFrameData frameData(encodedImage);
    
    // 1. set frame number prefix
    Name prefix = *framePrefix_;    
    shared_ptr<const vector<unsigned char>> frameNumberComponent = NdnRtcNamespace::getFrameNumberComponent(frameNo_);
    
    prefix.addComponent(*frameNumberComponent);

    // check whether frame is larger than allowed segment size
    int bytesToSend, payloadSize = frameData.getLength();
    double timeStampMS = NdnRtcUtils::timestamp();
    
    // 2. prepare variables for caomputing segment prefix
    unsigned long segmentNo = 0,
    // calculate total number of segments for current frame
    segmentsNum = NdnRtcUtils::getSegmentsNumber(segmentSize_, payloadSize);
    
    try {
        if (segmentsNum > 0)
        {
            // 4. split frame into segments
            // 5. publish segments under <root>/frameNo/segmentNo URI
            while (payloadSize > 0)
            {
                Name segmentPrefix = prefix;
                segmentPrefix.appendSegment(segmentNo);
                
                Data data(segmentPrefix);
                
                bytesToSend = (payloadSize < segmentSize_)? payloadSize : segmentSize_;
                payloadSize -= segmentSize_;
                
                data.getMetaInfo().setFreshnessSeconds(freshnessInterval_);
                data.getMetaInfo().setFinalBlockID(Name().appendSegment(segmentsNum-1).get(0).getValue());
                data.getMetaInfo().setTimestampMilliseconds(timeStampMS);
                data.setContent((const unsigned char *)&frameData.getData()[segmentNo*segmentSize_], bytesToSend);
                
                ndnKeyChain_->sign(data, *certificateName_);
                
                Blob encodedData = data.wireEncode();
                ndnTransport_->send(*encodedData);
                
                segmentNo++;
#if 0
                for (unsigned int i = 0; i < data.getContent().size(); ++i)
                    printf("%2x ",(*data.getContent())[i]);
                cout << endl;
#endif
            }
        }
        
    }
    catch (std::exception &e)
    {
        notifyError(-1, "got error from ndn library: %s", e.what());
        return;
    }
    
    frameNo_++;
}

//********************************************************************************
#pragma mark - public
int NdnVideoSender::init(const shared_ptr<ndn::Transport> transport)
{
    certificateName_ = NdnRtcNamespace::certificateNameForUser(((VideoSenderParams*)params_)->getUserPrefix());
    
    string framesPrefix = ((VideoSenderParams*)params_)->getStreamFramePrefix();
    framePrefix_.reset(new Name(framesPrefix.c_str()));

    ndnTransport_ = transport;
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(((VideoSenderParams*)params_)->getUserPrefix());
    
    if (((VideoSenderParams*)params_)->getSegmentSize(&segmentSize_) < 0)
        segmentSize_ = 3800;
    
    if (((VideoSenderParams*)params_)->getFreshnessInterval(&freshnessInterval_) < 0)
        freshnessInterval_ = 1;
        
    return 0;
}

//********************************************************************************
#pragma mark - private
