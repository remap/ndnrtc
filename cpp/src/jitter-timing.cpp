//
//  jitter-timing.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 5/8/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "jitter-timing.h"
#include "ndnrtc-utils.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace ndnlog;
using namespace std;

//******************************************************************************
#pragma mark - construction/destruction
JitterTiming::JitterTiming():
playoutTimer_(*EventWrapper::Create())
{
    resetData();
}

JitterTiming::~JitterTiming(){
    playoutTimer_.~EventWrapper();
}

//******************************************************************************
#pragma mark - public
void JitterTiming::flush()
{
    resetData();
    //    stop();
}
void JitterTiming::stop()
{
    playoutTimer_.StopTimer();
    playoutTimer_.Set();
}

int64_t JitterTiming::startFramePlayout()
{
    int64_t processingStart = NdnRtcUtils::microsecondTimestamp();
    LogTraceC << "processing start " << processingStart << endl;
    
    if (playoutTimestampUsec_ == 0)
    {
        LogTraceC << "init jitter timing" << endl;
        playoutTimestampUsec_ = processingStart;
    }
    else
    { // calculate processing delay from the previous iteration
        int64_t prevIterationProcTimeUsec = processingStart -
        playoutTimestampUsec_;
        
        LogTraceC << "prev iteration time " << prevIterationProcTimeUsec << endl;
        
        // substract frame playout delay
        if (prevIterationProcTimeUsec >= framePlayoutTimeMs_*1000)
            prevIterationProcTimeUsec -= framePlayoutTimeMs_*1000;
        else
            // should not occur!
            assert(0);
        
        LogTraceC << "prev iter proc time " << prevIterationProcTimeUsec << endl;
        
        // add this time to the average processing time
        processingTimeUsec_ += prevIterationProcTimeUsec;
        LogTraceC << "proc time " << processingTimeUsec_ << endl;
        
        playoutTimestampUsec_ = processingStart;
    }
    
    return playoutTimestampUsec_;
}

void JitterTiming::updatePlayoutTime(int framePlayoutTime)
{
    LogTraceC << "producer playout time " << framePlayoutTime << endl;
    
    int playoutTimeUsec = framePlayoutTime*1000;
    if (playoutTimeUsec < 0) playoutTimeUsec = 0;
    
    if (processingTimeUsec_ >= 1000)
    {
        LogTraceC <<"accomodate processing time " << processingTimeUsec_ << endl;
        
        int processingUsec = (processingTimeUsec_/1000)*1000;
        
        LogTraceC << "processing " << processingUsec << endl;
        
        if (processingUsec > playoutTimeUsec)
        {
            LogTraceC
            << "skipping frame. processing " << processingUsec
            << " playout " << playoutTimeUsec << endl;
            
            processingUsec = playoutTimeUsec;
            playoutTimeUsec = 0;
        }
        else
            playoutTimeUsec -= processingUsec;
        
        processingTimeUsec_ = processingTimeUsec_ - processingUsec;
        LogTraceC
        << "playout usec " << playoutTimeUsec
        << " processing " << processingTimeUsec_ << endl;
    }
    
    framePlayoutTimeMs_ = playoutTimeUsec/1000;
}

void JitterTiming::runPlayoutTimer()
{
    assert(framePlayoutTimeMs_ >= 0);
    if (framePlayoutTimeMs_ > 0)
    {
        LogTraceC << "timer wait " << framePlayoutTimeMs_ << endl;
        
        playoutTimer_.StartTimer(false, framePlayoutTimeMs_);
        playoutTimer_.Wait(WEBRTC_EVENT_INFINITE);
        LogTraceC << "timer done " << endl;
    }
    else
        LogTraceC << "skipping frame" << endl;
}

//******************************************************************************
void JitterTiming::resetData()
{
    framePlayoutTimeMs_ = 0;
    processingTimeUsec_ = 0;
    playoutTimestampUsec_ = 0;
}
