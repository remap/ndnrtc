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

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
MediaSender::MediaSender(const ParamsStruct &params) :
NdnRtcObject(params),
packetNo_(0)
{
    dataRateMeter_ = NdnRtcUtils::setupDataRateMeter(10);
    packetRateMeter_ = NdnRtcUtils::setupFrequencyMeter();
}

MediaSender::~MediaSender()
{
    NdnRtcUtils::releaseDataRateMeter(dataRateMeter_);
    NdnRtcUtils::releaseFrequencyMeter(packetRateMeter_);
}

//******************************************************************************
//******************************************************************************
#pragma mark - static
int MediaSender::getUserPrefix(const ParamsStruct &params, string &prefix)
{
    int res = RESULT_OK;
    
    res = (params.ndnHub && params.producerId) ? RESULT_OK : RESULT_ERR;
    
    prefix = *NdnRtcNamespace::
    getProducerPrefix((params.ndnHub)?params.ndnHub:DefaultParams.ndnHub,
                      (params.producerId)?params.producerId:
                      DefaultParams.producerId);
    
    return res;
}

int MediaSender::getStreamPrefix(const ParamsStruct &params, string &prefix)
{
    string userPrefix;
    int res = MediaSender::getUserPrefix(params, userPrefix);
    
    res = (params.streamName) ? res : RESULT_ERR;
    
    const string streamName = string((!res)?params.streamName:
                                     DefaultParams.streamName);
    
    prefix = *NdnRtcNamespace::buildPath(false,
                                        &userPrefix,
                                        &NdnRtcNamespace::NdnRtcNamespaceComponentUserStreams,
                                        &streamName,
                                        NULL);
    
    return res;
}

int MediaSender::getStreamFramePrefix(const ParamsStruct &params, string &prefix)
{
    string streamPrefix;
    int res = MediaSender::getStreamPrefix(params, streamPrefix);
    
    res = (params.streamThread) ? res : RESULT_ERR;
    
    string streamThread = string((!res)?params.streamThread:DefaultParams.streamThread);
    
    prefix = *NdnRtcNamespace::buildPath(false,
                                        &streamPrefix,
                                        &streamThread,
                                        &NdnRtcNamespace::NdnRtcNamespaceComponentStreamFrames,
                                        NULL);
    
    return res;
}

int MediaSender::getStreamKeyPrefix(const ParamsStruct &params, string &prefix)
{
    string streamPrefix;
    int res = MediaSender::getStreamPrefix(params, streamPrefix);
    
    prefix = *NdnRtcNamespace::buildPath(false,
                                        &streamPrefix,
                                        &NdnRtcNamespace::NdnRtcNamespaceComponentStreamKey,
                                        NULL);
    
    return res;
}

//******************************************************************************
#pragma mark - public
int MediaSender::init(const shared_ptr<ndn::Transport> transport)
{
    string userPrefix;
    
    if (RESULT_FAIL(MediaSender::getStreamPrefix(params_, userPrefix)))
        notifyError(-1, "user prefix differs from specified (check params): %s",
                          userPrefix.c_str());
    
    certificateName_ = NdnRtcNamespace::certificateNameForUser(userPrefix);

    string packetPrefix;
    
    if (RESULT_FAIL(MediaSender::getStreamFramePrefix(params_, packetPrefix)))
        notifyError(-1, "frame prefix differ from specified (check params): %s",
                    packetPrefix.c_str());

    packetPrefix_.reset(new Name(packetPrefix.c_str()));
    
    ndnTransport_ = transport;
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(userPrefix);
    segmentSize_ = params_.segmentSize;
    freshnessInterval_ = params_.freshness;
    
    packetNo_ = 0;
    
    return 0;
}

//******************************************************************************
#pragma mark - private
int MediaSender::publishPacket(unsigned int len,
                               const unsigned char *packetData,
                               shared_ptr<Name> packetPrefix,
                               unsigned int packetNo)
{
    if (!ndnTransport_->getIsConnected())
        return notifyError(-1, "can't send packet - no connection");
    
    // send packet over NDN
    
    // 1. set packet number prefix
    Name prefix = *packetPrefix;
    shared_ptr<const vector<unsigned char>> packetNumberComponent = NdnRtcNamespace::getNumberComponent(packetNo);
    
    prefix.addComponent(*packetNumberComponent);
    
    // check whether frame is larger than allowed segment size
    int bytesToSend, payloadSize = len;
    double timeStampMS = NdnRtcUtils::timestamp();
    
    // 2. prepare variables for computing segment prefix
    unsigned long segmentNo = 0;
    // calculate total number of segments for current frame
    unsigned long segmentsNum = NdnRtcUtils::getSegmentsNumber(segmentSize_, payloadSize);
    
    try {
        if (segmentsNum > 0)
        {
//            NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_, payloadSize_);
            
            // 4. split frame into segments
            // 5. publish segments under <root>/packetNo/segmentNo URI
            while (payloadSize > 0)
            {
                Name segmentPrefix = prefix;
                segmentPrefix.appendSegment(segmentNo);
                
                TRACE("sending packet #%010lld. prefix: %s", packetNo_,
                      segmentPrefix.toUri().c_str());
                
                Data data(segmentPrefix);
                
                bytesToSend = (payloadSize < segmentSize_)?
                                                        payloadSize :
                                                        segmentSize_;
                payloadSize -= segmentSize_;
                
                data.getMetaInfo().setFreshnessSeconds(freshnessInterval_);
                data.getMetaInfo().setFinalBlockID(Name().
                                                   appendSegment(segmentsNum-1).
                                                   get(0).getValue());
                
                data.getMetaInfo().setTimestampMilliseconds(timeStampMS);
                data.setContent(&packetData[segmentNo*segmentSize_],
                                bytesToSend);
                
                ndnKeyChain_->sign(data, *certificateName_);
                
                SignedBlob encodedData = data.wireEncode();
                ndnTransport_->send(*encodedData);
                
                segmentNo++;
                NdnRtcUtils::dataRateMeterMoreData(dataRateMeter_,
                                                   data.getContent().size());
#if 0
                for (unsigned int i = 0; i < data.getContent().size(); ++i)
                    printf("%2x ",(*data.getContent())[i]);
                cout << endl;
#endif
            }
        }
        else
            TRACE("\tNO PUBLISH: \t%d \t%d \%d",
                              segmentsNum,
                              segmentSize_,
                              payloadSize);
            
    }
    catch (std::exception &e)
    {
        return notifyError(-1,
                    "got error from ndn library while sending data: %s",
                    e.what());
    }

    return segmentsNum;
}
