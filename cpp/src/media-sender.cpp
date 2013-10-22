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
//******************************************************************************
#pragma mark - public
string MediaSenderParams::getUserPrefix() const
{
    const std::string hub = getParamAsString(ParamNameNdnHub);
    const std::string producerId = getParamAsString(ParamNameProducerId);
    
    shared_ptr<string> userPrefix = NdnRtcNamespace::getProducerPrefix(hub, producerId);
    return *userPrefix;
}

string MediaSenderParams::getStreamPrefix() const
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
    
    shared_ptr<string> prefix(NULL);
    
    // check if stream name was supplied
    if (strlen(stream) > 0)
        prefix = NdnRtcNamespace::getStreamPath(hub, user, stream);
    
    free(hub);
    free(user);
    free(stream);
    
    return (prefix.get())? *prefix : "";
}

string MediaSenderParams::getStreamKeyPrefix() const
{
    string prefix  = getStreamPrefix();
    
    if (prefix == "")
        return "";
    
    shared_ptr<string> accessPrefix =
    NdnRtcNamespace::buildPath(false, &prefix,
                               &NdnRtcNamespace::NdnRtcNamespaceComponentStreamKey, NULL);
    
    return *accessPrefix;
}

string MediaSenderParams::getStreamFramePrefix() const
{
    string prefix  = getStreamPrefix();
    string streamThread = getParamAsString(ParamNameStreamThreadName);
    
    shared_ptr<string> framesPrefix =
    NdnRtcNamespace::buildPath(false,
                               &prefix,
                               &streamThread,
                               &NdnRtcNamespace::NdnRtcNamespaceComponentStreamFrames,
                               NULL);
    
    return *framesPrefix;
}

//******************************************************************************
//******************************************************************************
#pragma mark - public
int MediaSender::init(const shared_ptr<ndn::Transport> transport)
{
    certificateName_ = NdnRtcNamespace::certificateNameForUser(getParams()->
                                                               getUserPrefix());
    
    string framesPrefix = getParams()->getStreamFramePrefix();
    packetPrefix_.reset(new Name(framesPrefix.c_str()));
    
    ndnTransport_ = transport;
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(getParams()->
                                                    getUserPrefix());
    
    if (getParams()->getSegmentSize(&segmentSize_) < 0)
        segmentSize_ = 1100;
    
    if (getParams()->getFreshnessInterval(&freshnessInterval_) < 0)
        freshnessInterval_ = 1;
    
    return 0;
}

//******************************************************************************
#pragma mark - intefaces realization - <#interface#>

//******************************************************************************
#pragma mark - private
int MediaSender::publishPacket(unsigned int len,
                                     const unsigned char *packetData)
{
    if (!ndnTransport_->getIsConnected())
        return notifyError(-1, "can't send packet - no connection");
    
    // send packet over NDN
    TRACE("sending packet #%010lld", packetNo_);
    
    // 1. set packet number prefix
    Name prefix = *packetPrefix_;
    shared_ptr<const vector<unsigned char>> packetNumberComponent = NdnRtcNamespace::getNumberComponent(packetNo_);
    
    prefix.addComponent(*packetNumberComponent);
    
    // check whether frame is larger than allowed segment size
    int bytesToSend, payloadSize = len;
    double timeStampMS = NdnRtcUtils::timestamp();
    
    // 2. prepare variables for computing segment prefix
    unsigned long segmentNo = 0,
    // calculate total number of segments for current frame
    segmentsNum = NdnRtcUtils::getSegmentsNumber(segmentSize_, payloadSize);
    
    try {
        if (segmentsNum > 0)
        {
            // 4. split frame into segments
            // 5. publish segments under <root>/packetNo/segmentNo URI
            while (payloadSize > 0)
            {
                Name segmentPrefix = prefix;
                segmentPrefix.appendSegment(segmentNo);
                
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
        return notifyError(-1,
                    "got error from ndn library while sending data: %s",
                    e.what());
    }
    
    packetNo_++;
    
}
