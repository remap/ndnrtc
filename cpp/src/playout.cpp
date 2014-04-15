//
//  playout.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 3/19/14
//

#include "playout.h"
#include "ndnrtc-utils.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

//******************************************************************************
#pragma mark - construction/destruction
Playout::Playout(const shared_ptr<const Consumer> &consumer):
isRunning_(false),
consumer_(consumer),
playoutThread_(*webrtc::ThreadWrapper::CreateThread(Playout::playoutThreadRoutine, this)),
data_(nullptr)
{
    setDescription("playout");
    jitterTiming_.flush();
    
    if (consumer_.get())
    {
        frameBuffer_ = consumer_->getFrameBuffer();
    }
}

Playout::~Playout()
{
    if (isRunning_)
        stop();
}

//******************************************************************************
#pragma mark - public
int
Playout::init(void* frameConsumer)
{
    jitterTiming_.flush();
    frameConsumer_ = frameConsumer;
}

int
Playout::start()
{
    nPlayed_ = 0;
    nMissed_ = 0;
    nIncomplete_ = 0;
    isRunning_ = true;
    
    unsigned int tid;
    playoutThread_.Start(tid);
    
    LogInfoC << "started" << endl;
    return RESULT_OK;
}

int
Playout::stop()
{
    playoutThread_.SetNotAlive();
    isRunning_ = false;
    playoutThread_.Stop();
    
    LogInfoC << "stopped" << endl;
    return RESULT_OK;
}

void
Playout::setLogger(ndnlog::new_api::Logger *logger)
{
    jitterTiming_.setLogger(logger);
    ILoggingObject::setLogger(logger);
}

void
Playout::setDescription(const std::string &desc)
{
    ILoggingObject::setDescription(desc);
    jitterTiming_.setDescription(NdnRtcUtils::toString("%s-timing",
                                                       getDescription().c_str()));
}

void
Playout::getStatistics(ReceiverChannelPerformance& stat)
{
    stat.nPlayed_ = nPlayed_;
    stat.nMissed_ = nMissed_;
    stat.nIncomplete_ = nIncomplete_;
}

//******************************************************************************
#pragma mark - private
bool
Playout::processPlayout()
{
    if (isRunning_)
    {
        int64_t now = NdnRtcUtils::millisecondTimestamp();
        
        if (frameBuffer_->getState() == FrameBuffer::Valid)
        {
            jitterTiming_.startFramePlayout();
            
            if (data_)
            {
                delete data_;
                data_ = nullptr;
            }
            
            PacketNumber packetNo;
            double assembledLevel = 0;
            bool isKey;
            
            frameBuffer_->acquireSlot(&data_, packetNo, isKey, assembledLevel);
            
            if (assembledLevel > 0 &&
                assembledLevel < 1)
            {
                nIncomplete_++;
                LogStatC
                << "\tincomplete\t" << nIncomplete_  << "\t"
                << (isKey?"K":"D") << "\t"
                << packetNo << "\t" << endl;
            }
            
            //******************************************************************
            // next call is overriden by specific playout mechanism - either
            // video or audio. the rest of the code is similar for both cases
            if (playbackPacket(now, data_, packetNo, assembledLevel, isKey))
            {
                nPlayed_++;
                
                LogStatC << "\tplay\t" << nPlayed_ << endl;
            }
            else
            {
                nMissed_++;
                
                LogStatC << "\tmissed\t" << nMissed_ << endl;
            }
            //******************************************************************
            
            // get playout time (delay) for the rendered frame
            int framePlayoutTime = frameBuffer_->releaseAcquiredSlot();
            
            assert(framePlayoutTime >= 0);
            
            LogTraceC
            << "playout time " << framePlayoutTime
            << " data: " << (data_?"YES":"NO") << endl;
            
            if (framePlayoutTime >= 0)
            {
                jitterTiming_.updatePlayoutTime(framePlayoutTime);
                
                // setup and run playout timer for calculated playout interval
                jitterTiming_.runPlayoutTimer();
            }
        }
    }
    
    return isRunning_;
}