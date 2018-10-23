//
//  simple-log.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/8/13
//

#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/make_shared.hpp>

#include <sys/time.h>

#if defined __APPLE__
#include <mach/mach_time.h>
#endif

#include <fstream>
#include <iomanip>
#include "simple-log.hpp"

#define MAX_BUF_SIZE 4*256 // string buffer

using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace boost::chrono;

static char tempBuf[MAX_BUF_SIZE];
#if defined __APPLE__
static std::string lvlToString[] = {
    [NdnLoggerLevelTrace] =     "TRACE",
    [NdnLoggerLevelDebug] =     "DEBUG",
    [NdnLoggerLevelInfo] =      "INFO ",
    [NdnLoggerLevelWarning] =   "WARN ",
    [NdnLoggerLevelError] =     "ERROR",
    [NdnLoggerLevelStat] =      "STAT "
};
#else
static std::string lvlToString[] = {
    [0] =     "TRACE",
    [1] =     "DEBUG",
    [2] =      "STAT ",
    [3] =      "INFO ",
    [4] =   "WARN ",
    [5] =     "ERROR"
};
#endif

static boost::asio::io_service LogIoService;
static boost::thread LogThread;
static boost::shared_ptr<boost::asio::io_service::work> LogThreadWork;

NilLogger NilLogger::nilLogger_;
Logger* Logger::sharedInstance_ = 0;

void startLogThread();
void stopLogThread();

//******************************************************************************
boost::recursive_mutex DefaultSink::stdOutMutex_;
std::map<std::string, boost::shared_ptr<Logger>> Logger::loggers_;

unsigned int Logger::FlushIntervalMs = 100;

#pragma mark - construction/destruction
Logger::Logger(const NdnLoggerDetailLevel& logLevel,
                        const std::string& logFile):
isWritingLogEntry_(false),
currentEntryLogType_(NdnLoggerLevelTrace),
logLevel_(logLevel),
sink_(boost::make_shared<DefaultSink>(logFile))
{
    lastFlushTimestampMs_ = getMillisecondTimestamp();   
    isProcessing_ = true;
}

Logger::Logger(const NdnLoggerDetailLevel& logLevel,
               const boost::shared_ptr<ILogRecordSink> sink):
isWritingLogEntry_(false),
currentEntryLogType_(NdnLoggerLevelTrace),
logLevel_(logLevel),
sink_(sink)
{
    lastFlushTimestampMs_ = getMillisecondTimestamp();   
    isProcessing_ = true;
}

Logger::~Logger()
{
    isProcessing_ = false;
}

//******************************************************************************
#pragma mark - public
Logger&
Logger::log(const NdnLogType& logType,
                     const ILoggingObject* loggingInstance,
                     const std::string& locationFunc,
                     const int& locationLine)
{
    
    if (logType < (NdnLogType)logLevel_)
        return NilLogger::get();
    
    sink_->lockExclusively();
    
    if (isWritingLogEntry_ &&
        currentEntryLogType_ >= (NdnLogType)logLevel_)
        throw std::runtime_error("Previous log entry wasn't closed");
    
    sink_->unlock();
    
    bool shouldIgnore = (loggingInstance != 0 &&
                            !loggingInstance->isLoggingEnabled());
    
    if (!shouldIgnore &&
        logType >= (NdnLogType)logLevel_)
    {
        sink_->lockExclusively();
        startLogRecord();
        
        isWritingLogEntry_ = true;
        currentEntryLogType_ = logType;
        
        // LogEntry header has the following format:
        // <timestamp> <log_level> - <logging_instance> [<location_file>:<location_line>] ":"
        // log location info is enabled only for debug levels less than INFO
        currentLogRecord_ << getMillisecondTimestamp() << "\t[" << stringify(logType) << "]";
        
        if (loggingInstance)
            currentLogRecord_
            << "[" << std::setw(20) << loggingInstance->getDescription() << "]-"
            << std::setw(20) << locationFunc;
        
        currentLogRecord_ << ": ";
    }
    
    return *this;
}

void
Logger::flush()
{
    sink_->flush();
    // getOutStream().flush();    
}

//******************************************************************************
#pragma mark - static
void
Logger::initAsyncLogging()
{
    startLogThread();
}

void
Logger::releaseAsyncLogging()
{
    stopLogThread();
}

Logger&
Logger::getLogger(const std::string &logFile)
{
    boost::shared_ptr<Logger> logger = Logger::getLoggerPtr(logFile);
    return *logger;
}

boost::shared_ptr<Logger>
Logger::getLoggerPtr(const std::string &logFile)
{
    std::map<std::string, boost::shared_ptr<Logger> >::iterator it = loggers_.find(logFile);
    
    if (it == loggers_.end())
        return loggers_[logFile] = boost::make_shared<Logger>(NdnLoggerDetailLevelAll, logFile);
    else
        return it->second;
    
    return boost::shared_ptr<Logger>();
}

void
Logger::destroyLogger(const std::string &logFile)
{
    std::map<std::string, boost::shared_ptr<Logger> >::iterator it = loggers_.find(logFile);
    
    if (it != loggers_.end())
        loggers_.erase(it);
}

//******************************************************************************
#pragma mark - private
std::string
Logger::stringify(NdnLoggerLevel lvl)
{
    return lvlToString[lvl];
}

int64_t
Logger::getMillisecondTimestamp()
{
    milliseconds msec = duration_cast<milliseconds>(system_clock::now().time_since_epoch());
    return msec.count();
}

void
Logger::processLogRecords()
{
    if (isProcessing_)
    {
        std::string record;

        while (recordsQueue_.pop(record))
            if (sink_) sink_->finalizeRecord(record);

        if ((getMillisecondTimestamp() - lastFlushTimestampMs_) >= FlushIntervalMs)
            if (sink_) sink_->flush();
    }
    else if (sink_)
    {
        sink_->flush();
        sink_->close();
    }
}

void
Logger::startLogRecord()
{
    currentLogRecord_.str( std::string() );
    currentLogRecord_.clear();
}

void
Logger::finalizeLogRecord()
{
    if (!recordsQueue_.push(currentLogRecord_.str()))
        sink_->finalizeRecord("[CRITICAL]\tlog queue is full");
        // getOutFileStream() << "[CRITICAL]\tlog queue is full" << std::endl;
    else
        LogIoService.post(boost::bind(&Logger::processLogRecords, shared_from_this()));
}

//******************************************************************************
void startLogThread()
{
    if (!LogThreadWork.get())
    {
        LogThreadWork.reset(new boost::asio::io_service::work(LogIoService));
        LogIoService.reset();
        LogThread = boost::thread([](){
            LogIoService.run();
        });
    }
}

void stopLogThread()
{
    if (LogThreadWork.get())
    {
        LogThreadWork.reset();
        LogIoService.stop();
        LogThread.try_join_for(boost::chrono::milliseconds(100));
    }
}


//******************************************************************************
DefaultSink::DefaultSink(const std::string& logFile):
logFile_(logFile),
outStream_(&std::cout)
{
    if (logFile != "")
    {
        outStream_ = new std::ofstream();
        getOutFileStream().open(logFile_.c_str(),
                                std::ofstream::out | std::ofstream::trunc);
    }
}

void
DefaultSink::finalizeRecord(const std::string& record)
{
    getOutFileStream() << record;
}

void 
DefaultSink::flush()
{
    if (&getOutStream() != &std::cout)
        getOutFileStream().flush();
}

void 
DefaultSink::close()
{
    if (&getOutStream() != &std::cout)
        getOutFileStream().close();    
}

//******************************************************************************
CallbackSink::CallbackSink(LoggerSinkCallback callback)
    : callback_(callback)
{
    triggerCallbackImpl_ = [this](const std::string& msg){
        callback_(msg.c_str());
    };
}

CallbackSink::CallbackSink(LoggerSinkCallbackFun callbackFun)
    : callbackFun_(callbackFun)
{
    triggerCallbackImpl_ = [this](const std::string& msg){
        callbackFun_(msg.c_str());
    };
}

CallbackSink::~CallbackSink(){}

void
CallbackSink::finalizeRecord(const std::string& record)
{
    // callback_(record.c_str()); 
    triggerCallbackImpl_(record);
}

void CallbackSink::flush(){}
void CallbackSink::close(){}
bool CallbackSink::isStdOut(){ return false; }
void CallbackSink::lockExclusively() { mutex_.lock(); }
void CallbackSink::unlock() { mutex_.unlock(); }
