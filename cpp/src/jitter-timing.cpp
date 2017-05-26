//
//  jitter-timing.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 5/8/14.
//  Copyright 2013-2015 Regents of the University of California
//

#include <boost/make_shared.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/function.hpp>

#include "jitter-timing.hpp"
#include "clock.hpp"
#include "simple-log.hpp"
#include "ndnrtc-object.hpp"

using namespace ndnrtc;
using namespace ndnlog;
using namespace std;

#if BOOST_ASIO_HAS_STD_CHRONO

namespace lib_chrono=std::chrono;

#else

namespace lib_chrono=boost::chrono;

#endif

namespace ndnrtc {
    class JitterTimingImpl : public NdnRtcComponent {
    public:
        JitterTimingImpl(boost::asio::io_service& io);

        void flush();
        void stop();
        int64_t startFramePlayout();
        void updatePlayoutTime(int framePlayoutTime);
        void run(boost::function<void()> callback);


    private:
        friend JitterTiming::~JitterTiming();

        boost::asio::steady_timer timer_;
        int framePlayoutTimeMs_ = 0;
        int processingTimeUsec_ = 0;
        int64_t playoutTimestampUsec_ = 0;

        void resetData();
    };
}

//******************************************************************************
JitterTiming::JitterTiming(boost::asio::io_service& io):
pimpl_(boost::make_shared<JitterTimingImpl>(io)){}
JitterTiming::~JitterTiming() { pimpl_->timer_.cancel(); }
void JitterTiming::flush() { pimpl_->flush(); }
void JitterTiming::stop() { pimpl_->stop(); }
int64_t JitterTiming::startFramePlayout() { return pimpl_->startFramePlayout(); }
void JitterTiming::updatePlayoutTime(int framePlayoutTime) { pimpl_->updatePlayoutTime(framePlayoutTime); }
void JitterTiming::run(boost::function<void()> callback) { pimpl_->run(callback); }
void JitterTiming::setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger) { pimpl_->setLogger(logger); }
void JitterTiming::setDescription(const std::string& desc) { pimpl_->setDescription(desc); }

//******************************************************************************
#pragma mark - public
JitterTimingImpl::JitterTimingImpl(boost::asio::io_service& io):timer_(io)
{
    resetData();
}

void JitterTimingImpl::flush()
{
    resetData();
    LogTraceC << "flushed" << std::endl;
}
void JitterTimingImpl::stop()
{
    timer_.cancel();
    resetData();
    LogTraceC << "stopped" << std::endl;
}

int64_t JitterTimingImpl::startFramePlayout()
{
    int64_t processingStart = clock::microsecondTimestamp();
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

void JitterTimingImpl::updatePlayoutTime(int framePlayoutTime)
{
    LogTraceC << ". packet playout time " << framePlayoutTime << endl;
    
    int playoutTimeUsec = framePlayoutTime*1000;
    assert(playoutTimeUsec >= 0);
    
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

void JitterTimingImpl::run(boost::function<void()> callback)
{
    assert(framePlayoutTimeMs_ >= 0);
    
    LogTraceC << ". timer wait " << framePlayoutTimeMs_ << " ]" << endl;
    
    timer_.expires_from_now(lib_chrono::microseconds(framePlayoutTimeMs_*1000));
    timer_.async_wait([callback](const boost::system::error_code& e){
        if (e != boost::asio::error::operation_aborted)
            callback();
    });
}

//******************************************************************************
void JitterTimingImpl::resetData()
{
    framePlayoutTimeMs_ = 0;
    processingTimeUsec_ = 0;
    playoutTimestampUsec_ = 0;
}
