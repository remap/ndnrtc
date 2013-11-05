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

#include <sys/time.h>

#include "simple-log.h"

#define MAX_BUF_SIZE 4*256 // string buffer

using namespace ndnlog;

static char tempBuf[MAX_BUF_SIZE];
static NdnLogger *sharedLogger = NULL;

//********************************************************************************
#pragma mark - construction/destruction
NdnLogger::NdnLogger(const char *logFile, NdnLoggerDetailLevel logDetailLevel):
loggingDetailLevel_(logDetailLevel),
outLogStream_(NULL),
logMutex_(PTHREAD_MUTEX_INITIALIZER),
logFile_("")
{
    buf_ = (char*)malloc(MAX_BUF_SIZE);
    flushBuffer(buf_);

    if (logFile)
    {
        outLogStream_ = fopen(logFile, "w");
        logFile_ = std::string(logFile);
    }

    if (!logFile || outLogStream_ <= 0)
    {
        outLogStream_ = stdout;
        logFile_ = "";
    }
}
NdnLogger::~NdnLogger()
{
    INFO("shutting down log session");
    
    if (outLogStream_ != stdout)
        fclose(outLogStream_);
    
    free(buf_);
    pthread_mutex_destroy(&logMutex_);
}

//********************************************************************************
#pragma mark - all static
void NdnLogger::initialize(const char *logFile, NdnLoggerDetailLevel logDetailLevel)
{
    if (sharedLogger)
        delete sharedLogger;
    
    sharedLogger = new NdnLogger(logFile, logDetailLevel);
}

NdnLogger* NdnLogger::getInstance()
{
    if (!sharedLogger)
        sharedLogger = new NdnLogger();
    
    return sharedLogger;
}

const char* NdnLogger::stingify(NdnLoggerLevel lvl)
{
    switch (lvl) {
        case NdnLoggerLevelDebug:
            return "DEBUG";
        case NdnLoggerLevelInfo:
            return "INFO";
        case NdnLoggerLevelWarning:
            return "WARN";
        case NdnLoggerLevelError:
            return "ERROR";
        case NdnLoggerLevelTrace:
            return "TRACE";
        default:
            return "INFO";
    }
    return 0;
}

void NdnLogger::flushBuffer(char *buffer)
{
    memset(buffer, 0, MAX_BUF_SIZE);    
}

void NdnLogger::log(const char *fName, NdnLoggerLevel level, const char *format, ...)
{
    NdnLogger *sharedInstance = NdnLogger::getInstance();
    
    if (level >= (NdnLoggerLevel)sharedInstance->getLoggingDetailLevel())
    {
        va_list args;
        
        va_start(args, format);
        sharedInstance->flushBuffer(tempBuf);
        vsprintf(tempBuf, format, args);
        va_end(args);
        
        char *buf = sharedInstance->getBuffer();
        
        sharedInstance->flushBuffer(buf);
        sprintf(buf, "[%s] %s: %s", NdnLogger::stingify(level),
                (level <= NdnLoggerLevelDebug)? fName: "" , tempBuf);
        
        sharedInstance->log(buf);
    }
}

std::string NdnLogger::currentLogFile()
{
    return sharedLogger->logFile_;
}

NdnLoggerDetailLevel NdnLogger::currentLogLevel()
{
    return sharedLogger->loggingDetailLevel_;
}


//********************************************************************************
#pragma mark - private
int64_t NdnLogger::millisecondTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    int64_t ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
    
    return ticks;
}

void NdnLogger::log(const char *str)
{
    struct timeval tv;
    static char timestamp[100];
    
    gettimeofday(&tv,NULL);
    
    sprintf(timestamp, "%ld.%-6d : ", tv.tv_sec, tv.tv_usec);
    
    // make exlusive by semaphores
    pthread_mutex_lock(&logMutex_);
    fprintf(outLogStream_, "%lld: %s\n", millisecondTimestamp(), str);
    
    if (outLogStream_ != stdout)
        fflush(outLogStream_);
    
    pthread_mutex_unlock(&logMutex_);
}
