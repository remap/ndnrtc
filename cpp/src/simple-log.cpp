//
//  simple-log.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 8/8/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include <sys/time.h>

#include "simple-log.h"

#define MAX_BUF_SIZE 4*256 // string buffer

using namespace ndnlog;

static char tempBuf[MAX_BUF_SIZE];
static NdnLogger *sharedLogger = 0;

//********************************************************************************
#pragma mark - construction/destruction
NdnLogger::NdnLogger()
{
    buf = (char*)malloc(MAX_BUF_SIZE);
    flushBuffer(buf);
}
NdnLogger::~NdnLogger()
{
    free(buf);
}

//********************************************************************************
#pragma mark - all static
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
    va_list args;
    
    va_start(args, format);
    sharedInstance->flushBuffer(tempBuf);
    vsprintf(tempBuf, format, args);
    va_end(args);
    
    char *buf = sharedInstance->getBuffer();
    
    sharedInstance->flushBuffer(buf);
    sprintf(buf, "[%s] %s: %s", NdnLogger::stingify(level), fName, tempBuf);
    
    sharedInstance->log(buf);
}

//********************************************************************************
#pragma mark - private
void NdnLogger::log(const char *str)
{
    struct timeval tv;
    static char timestamp[100];
    
    gettimeofday(&tv,NULL);
    
    sprintf(timestamp, "%ld:%d : ", tv.tv_sec, tv.tv_usec);
    
    std::cout<<timestamp<<str<<std::endl;
}
