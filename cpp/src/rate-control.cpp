//
//  rate-control.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "rate-control.h"
#include "realtime-arc.h"
#include "video-sender.h"
#include "consumer.h"
#include "pipeliner.h"
#include "rtt-estimation.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

RateControl::RateControl(const shared_ptr<Consumer>& consumer):
streamId_(0),
consumer_(consumer),
arcWatchingThread_(*webrtc::ThreadWrapper::CreateThread(RateControl::arcWatchingRoutine, this)),
arcWatchTimer_(*webrtc::EventWrapper::Create())
{
    setDescription("rate-control");
}

int
RateControl::initialize(const ParamsStruct& params)
{
    int res = RESULT_OK;
    arcModule_.reset(new RealTimeAdaptiveRateControl());
    
    StreamEntry *streamsArray = nullptr;
    streamArrayForArcModule(params, &streamsArray);
    
    if (arcModule_->initialize(CodecModeNormal, params.nStreams, streamsArray) < 0)
    {
        res = RESULT_ERR;
        LogErrorC << "couldn't initialize ARC module" << endl;
    }
    
    free(streamsArray);
    
    return res;
}

void
RateControl::start()
{
    LogDebugC
    << "arc thread started" << endl;
    
    consumer_->getPipeliner()->switchToStream(streamId_);
    
    unsigned int tid;
    arcWatchingThread_.Start(tid);
}

void
RateControl::stop()
{
    LogDebugC
    << "arc thread stopped" << endl;
    
    arcWatchTimer_.StopTimer();
    arcWatchTimer_.Set();
    arcWatchingThread_.Stop();
    arcWatchingThread_.SetNotAlive();
}

void
RateControl::interestExpressed(const shared_ptr<const Interest>& interest)
{
    if (consumer_->getPipeliner()->getState() == Pipeliner::StateFetching)
        arcModule_->interestExpressed(interest->getName().toUri(), streamId_);
}

void
RateControl::interestTimeout(const shared_ptr<const Interest>& interest)
{
    arcModule_->interestTimeout(interest->getName().toUri(), streamId_);
}

void
RateControl::dataReceived(const ndn::Data& data, unsigned int nRtx)
{
    if (consumer_->getPipeliner()->getState() == Pipeliner::StateFetching)
    {
        arcModule_->dataReceived(data.getName().toUri(),
                                 streamId_,
                                 data.getContent().size(),
                                 consumer_->getRttEstimation()->getCurrentEstimation(),
                                 nRtx);
    }
}

void
RateControl::streamArrayForArcModule(const ParamsStruct &params,
                                          StreamEntry **streamArray)
{
    *streamArray = (StreamEntry*)malloc(sizeof(StreamEntry)*params.nStreams);
    memset(*streamArray, 0, sizeof(StreamEntry)*params.nStreams);
    
    for (int i = 0; i < params.nStreams; i++)
    {
        (*streamArray)[i].id_ = params.streamsParams[i].idx;
        (*streamArray)[i].bitrate_ = params.streamsParams[i].startBitrate*1000;
        (*streamArray)[i].parityRatio_ = NdnVideoSender::ParityRatio;
    }
}

bool
RateControl::checkArcStatus()
{
    int64_t timestamp = NdnRtcUtils::millisecondTimestamp();
    double interestRate = 0;
    unsigned int streamId = 0;
    
    arcModule_->getInterestRate(timestamp, interestRate, streamId);
    
    if (streamId_ != streamId && streamId < consumer_->getParameters().nStreams)
    {
        LogTrace("arc.log") << "switching "
        << streamId_ << " -> " << streamId << endl;
        
        streamId_ = streamId;
        consumer_->getPipeliner()->switchToStream(streamId_);
    }
    
    LogTrace("arc.log") << STAT_DIV
    << "interest rate " << STAT_DIV << interestRate << STAT_DIV
    << "stream id" << STAT_DIV << streamId << endl;
    
    arcWatchTimer_.StartTimer(false, 50);
    arcWatchTimer_.Wait(WEBRTC_EVENT_INFINITE);
    
    return true;
}