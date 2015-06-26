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
playoutTimer_(io_service_)
{
    resetData();
}

JitterTiming::~JitterTiming(){
}

//******************************************************************************
#pragma mark - public
void JitterTiming::flush()
{
    resetData();
    LogTraceC << "flushed" << std::endl;
}
void JitterTiming::stop()
{
    LogTraceC << "stopped" << std::endl;
}

int64_t JitterTiming::startFramePlayout()
{
    int64_t processingStart = NdnRtcUtils::microsecondTimestamp();
    LogTraceC << "[ proc start " << processingStart << endl;
    
    if (playoutTimestampUsec_ == 0)
    {
        playoutTimestampUsec_ = processingStart;
    }
    else
    { // calculate processing delay from the previous iteration
        int64_t prevIterationProcTimeUsec = processingStart -
        playoutTimestampUsec_;
        
        LogTraceC << ". prev iter full time " << prevIterationProcTimeUsec << endl;
        
        // substract frame playout delay
        if (prevIterationProcTimeUsec >= framePlayoutTimeMs_*1000)
            prevIterationProcTimeUsec -= framePlayoutTimeMs_*1000;
        else
            // should not occur!
        {
            LogErrorC << "assertion failed: "
            << "prevIterationProcTimeUsec ("
            << prevIterationProcTimeUsec << ") < framePlayoutTimeMs_*1000"
            << "(" << framePlayoutTimeMs_*1000 << ")" << endl;
            
            logger_->flush();
            
            assert(0);
        }
        
        LogTraceC << ". prev iter proc time " << prevIterationProcTimeUsec << endl;
        
        // add this time to the average processing time
        processingTimeUsec_ += prevIterationProcTimeUsec;
        LogTraceC << ". total proc time " << processingTimeUsec_ << endl;
        
        playoutTimestampUsec_ = processingStart;
    }
    
    return playoutTimestampUsec_;
}

void JitterTiming::updatePlayoutTime(int framePlayoutTime, PacketNumber packetNo)
{
    LogTraceC << ". packet " << packetNo << " playout time " << framePlayoutTime << endl;
    
    int playoutTimeUsec = framePlayoutTime*1000;
    if (playoutTimeUsec < 0) playoutTimeUsec = 0;
    
    if (processingTimeUsec_ >= 1000)
    {
        LogTraceC << ". absorb proc time " << processingTimeUsec_ << endl;
        
        int processingUsec = (processingTimeUsec_/1000)*1000;
        
        LogTraceC << ". proc absorb part " << processingUsec << endl;
        
        if (processingUsec > playoutTimeUsec)
        {
            LogTraceC
            << ". skip frame. proc " << processingUsec
            << " playout " << playoutTimeUsec << endl;
            
            processingUsec = playoutTimeUsec;
            playoutTimeUsec = 0;
        }
        else
            playoutTimeUsec -= processingUsec;
        
        processingTimeUsec_ = processingTimeUsec_ - processingUsec;
        LogTraceC
        << ". playout usec " << playoutTimeUsec
        << " total proc " << processingTimeUsec_ << endl;
    }
    
    framePlayoutTimeMs_ = playoutTimeUsec/1000;
}

void JitterTiming::runPlayoutTimer()
{
    assert(framePlayoutTimeMs_ >= 0);
    if (framePlayoutTimeMs_ > 0)
    {
        LogTraceC << ". timer wait " << framePlayoutTimeMs_ << endl;

        playoutTimer_.expires_from_now(boost::chrono::milliseconds(framePlayoutTimeMs_));
        playoutTimer_.wait();
        LogTraceC << "timer done]" << endl;
    }
    else
        LogTraceC << "skipping frame]" << endl;
}

//******************************************************************************
void JitterTiming::resetData()
{
    framePlayoutTimeMs_ = 0;
    processingTimeUsec_ = 0;
    playoutTimestampUsec_ = 0;
}
