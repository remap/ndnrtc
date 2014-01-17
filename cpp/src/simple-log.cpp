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
#include <mach/mach_time.h>

#include "simple-log.h"

#define MAX_BUF_SIZE 4*256 // string buffer

using namespace ndnlog;

static char tempBuf[MAX_BUF_SIZE];
static NdnLogger *sharedLogger = NULL;

pthread_mutex_t NdnLogger::logMutex_(PTHREAD_MUTEX_INITIALIZER);

//********************************************************************************
#pragma mark - construction/destruction
NdnLogger::NdnLogger(NdnLoggerDetailLevel logDetailLevel, const char *logFileFmt, ...):
loggingDetailLevel_(logDetailLevel),
outLogStream_(NULL),
logFile_(""),
instanceMutex_(PTHREAD_MUTEX_INITIALIZER)
{
    char *logFile = (char*)malloc(256);
    memset((void*)logFile, 0, 256);
    
    va_list args;
    
    va_start(args, logFileFmt);
    vsprintf(logFile, logFileFmt, args);
    va_end(args);
    
    buf_ = (char*)malloc(MAX_BUF_SIZE);
    flushBuffer(buf_);
    
    outLogStream_ = fopen(logFile, "w");
    logFile_ = std::string(logFile);
    lastFileFlush_ = millisecondTimestamp();

    free(logFile);
}

NdnLogger::NdnLogger(const char *logFile, NdnLoggerDetailLevel logDetailLevel):
loggingDetailLevel_(logDetailLevel),
outLogStream_(NULL),
logFile_(""),
instanceMutex_(PTHREAD_MUTEX_INITIALIZER)
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
  else
    lastFileFlush_ = millisecondTimestamp();
  
}
NdnLogger::~NdnLogger()
{
  LOG_INFO("shutting down log session");
  
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
  {
    // wait for unlocking
    pthread_mutex_lock(&logMutex_);
    pthread_mutex_unlock(&logMutex_);
    pthread_mutex_destroy(&logMutex_);
    
    delete sharedLogger;
  }
  
  pthread_mutex_init(&logMutex_, NULL);
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
  pthread_mutex_lock(&logMutex_);
  
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
            (level < NdnLoggerLevelTrace)? fName: "" , tempBuf);
    
    sharedInstance->log(buf);
  }
  
  pthread_mutex_unlock(&logMutex_);
}

std::string NdnLogger::currentLogFile()
{
  return sharedLogger->logFile_;
}

NdnLoggerDetailLevel NdnLogger::currentLogLevel()
{
  return sharedLogger->loggingDetailLevel_;
}

//******************************************************************************
#pragma mark - public
void NdnLogger::logString(const char *str)
{
  log(str);
}

void NdnLogger::log(NdnLoggerLevel level, const char *format, ...)
{
  if (level >= (NdnLoggerLevel)getLoggingDetailLevel())
  {
    pthread_mutex_lock(&instanceMutex_);
    va_list args;
    
    va_start(args, format);
    flushBuffer(buf_);
    vsprintf(buf_, format, args);
    va_end(args);
    
    log(buf_);
    pthread_mutex_unlock(&instanceMutex_);
  }
  
}
//********************************************************************************
#pragma mark - private
int64_t NdnLogger::millisecondTimestamp()
{
#if 0
  struct timeval tv;
  gettimeofday(&tv, NULL);
  
  int64_t ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
  
  return ticks;
#endif
  
  struct timeval tv;
  gettimeofday(&tv, NULL);
  
  int64_t ticks = 0;
  
#if 0
  ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
#else
  static mach_timebase_info_data_t timebase;
  if (timebase.denom == 0) {
    // Get the timebase if this is the first time we run.
    // Recommended by Apple's QA1398.
    kern_return_t retval = mach_timebase_info(&timebase);
    if (retval != KERN_SUCCESS) {
      // TODO(wu): Implement CHECK similar to chrome for all the platforms.
      // Then replace this with a CHECK(retval == KERN_SUCCESS);
      asm("int3");
    }
  }
  // Use timebase to convert absolute time tick units into nanoseconds.
  ticks = mach_absolute_time() * timebase.numer / timebase.denom;
  ticks /= 1000000LL;
#endif
  
  return ticks;
}

void NdnLogger::log(const char *str)
{
  // make exlusive by semaphores
  
  fprintf(outLogStream_, "%lld\t%s\n", millisecondTimestamp(), str);
  
  if (outLogStream_ != stdout)
  {
    // do not flush too fast
    if (millisecondTimestamp() - lastFileFlush_ >= 100)
      fflush(outLogStream_);
  }
}

//******************************************************************************
//******************************************************************************
// LoggerObject
//void LoggerObject::initializeLogger(const char *format, ...)
//{
//    char *logFile = (char*)malloc(256);
//    memset((void*)logFile, 0, 256);
//    
//    va_list args;
//    
//    va_start(args, format);
//    vsprintf(logFile, format, args);
//    va_end(args);
//    
//    this->setLogger(new NdnLogger(logFile, NdnLoggerDetailLevelAll));
//    this->isLoggerCreated_ = true;
//    
//    free(logFile);
//}


