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
segmentizer_(params),
packetNo_(0)
{
    packetRateMeter_ = NdnRtcUtils::setupFrequencyMeter();
}

MediaSender::~MediaSender()
{
    NdnRtcUtils::releaseFrequencyMeter(packetRateMeter_);
}

//******************************************************************************
//******************************************************************************
#pragma mark - static


//******************************************************************************
#pragma mark - public
int MediaSender::init(const shared_ptr<Face> &face,
                      const shared_ptr<ndn::Transport> &transport)
{
    if (RESULT_FAIL(segmentizer_.init(face, transport)))
        return notifyError(-1, "could not init segmentizer");
    
    shared_ptr<string> packetPrefix = NdnRtcNamespace::getStreamFramePrefix(params_);
    
    if (!packetPrefix.get())
        notifyError(-1, "bad frame prefix");
    
    packetPrefix_.reset(new Name(packetPrefix->c_str()));
    
    segmentSize_ = params_.segmentSize;
    freshnessInterval_ = params_.freshness;
    packetNo_ = 0;
    
    return RESULT_OK;
}

//******************************************************************************
#pragma mark - private
int MediaSender::publishPacket(unsigned int len,
                               const unsigned char *packetData,
                               shared_ptr<Name> packetPrefix,
                               unsigned int packetNo)
{
    // send packet over NDN
    
    // 1. set packet number prefix
    Name prefix = *packetPrefix;
    shared_ptr<const vector<unsigned char>> packetNumberComponent = NdnRtcNamespace::getNumberComponent(packetNo);
    
    prefix.append(*packetNumberComponent);
    
    // check whether frame is larger than allowed segment size
    int bytesToSend, payloadSize = len;
    
    // 2. prepare variables for computing segment prefix
    unsigned long segmentNo = 0;
    // calculate total number of segments for current frame
    unsigned long segmentsNum = NdnRtcUtils::getSegmentsNumber(segmentSize_, payloadSize);
    
    try {
        if (segmentsNum > 0)
        {
            // 4. split frame into segments
            // 5. publish segments under <root>/packetNo/segmentNo URI
            while (payloadSize > 0)
            {
                Name segmentName = prefix;
                segmentName.appendSegment(segmentNo);
                segmentName.appendFinalSegment(segmentsNum-1);
                
                TRACE("sending packet #%010lld. data name: %s", packetNo_,
                      segmentName.toUri().c_str());
                
                bytesToSend = (payloadSize < segmentSize_)? payloadSize : segmentSize_;
                payloadSize -= segmentSize_;

                segmentizer_.publishData(segmentName,
                                         &packetData[segmentNo*segmentSize_],
                                         bytesToSend,
                                         freshnessInterval_);
                segmentNo++;
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
