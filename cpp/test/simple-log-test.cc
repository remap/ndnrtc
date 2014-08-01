//
//  simple-log-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 2/28/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "simple-log.h"
#include "ndnrtc-utils.h"

using namespace ndnlog::new_api;

//::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(SimpleLogTests, TestLogToConsole)
{
    { // log default
        Logger logger;
        
        logger.log(NdnLoggerLevelInfo)
            << "you should be able to see this line in the console.";
        logger << " adding number: " << 1234;
        logger << ". ending line" << std::endl;
        
        logger.log(NdnLoggerLevelInfo) << "this is a new logging line and it ends here" << std::endl;
        logger.log(NdnLoggerLevelInfo) << "yet another new log line" << std::endl;
    }
    { // log with levels
        Logger logger;
        
        logger.log(NdnLoggerLevelTrace) << "trace level";
        logger.log(NdnLoggerLevelDebug) << "debug level";
        logger.log(NdnLoggerLevelInfo) << "info level";
        logger.log(NdnLoggerLevelWarning) << "warning level";
        logger.log(NdnLoggerLevelError) << "error level" << std::endl;
    }
    { // log with default level
        Logger logger(NdnLoggerDetailLevelDefault);
        
        logger.log(NdnLoggerLevelTrace) << "trace level - should be ignored";
        logger.log(NdnLoggerLevelDebug) << "debug level - should be ignored";
        logger.log(NdnLoggerLevelInfo) << "info level";
        logger.log(NdnLoggerLevelWarning) << "warning level";
        logger.log(NdnLoggerLevelError) << "error level" << std::endl;
    }
    { // logger level none
        Logger logger(NdnLoggerDetailLevelNone);
        
        logger.log(NdnLoggerLevelTrace) << "trace level - should be ignored";
        logger.log(NdnLoggerLevelDebug) << "debug level - should be ignored";
        logger.log(NdnLoggerLevelInfo) << "info level - should be ignored";
        logger.log(NdnLoggerLevelWarning) << "warning level - should be ignored";
        logger.log(NdnLoggerLevelError) << "error level - should be ignored";
    }
    { // test trace and debug
        Logger logger;
        
        logger.log(NdnLoggerLevelTrace, nullptr, __FILE__, __LINE__) << "trace entry at this file and line";
        logger.log(NdnLoggerLevelDebug, nullptr, __FILE__, __LINE__) << "no info on sources for debug level" << std::endl;
    }
}

class TestLoggingObject : public ILoggingObject
{
public:
    std::string getDescription() const {
        return "test-object";
    }
    
    bool isLoggingEnabled() const
    {
        return loggingEnabled_;
    }
    
    void setLoggingEnabled(bool isEnabled)
    {
        loggingEnabled_ = isEnabled;
    }
    
private:
    bool loggingEnabled_ = true;
};

TEST(SimpleLogTests, TestWithLoggingObject)
{
    {
        TestLoggingObject obj;
        Logger logger;
        
        logger.log(NdnLoggerLevelInfo, &obj) << "logging from object";
        // disable logging
        obj.setLoggingEnabled(false);
        logger.log(NdnLoggerLevelInfo, &obj) << "this line should be ignored";
    }
}

TEST(SimpleLogTests, TestLogToFile)
{
    { // log to file
        std::string
        fileName = ndnrtc::NdnRtcUtils::toString("logfile-%s-%d.log", "simple", 1);
        
        Logger logger(NdnLoggerDetailLevelAll, fileName);
        
        EXPECT_TRUE(UnitTestHelper::checkFileExists(fileName.c_str()));
        
        logger.log(NdnLoggerLevelInfo) << "test log to file " << 1234;
        
        TestLoggingObject obj;
        logger.log(NdnLoggerLevelInfo, &obj) << "log from object" << std::endl;
    }
}

TEST(SimpleLogTests, TestLogInvalid)
{
    {
        Logger logger;
        
        logger << "this line should be ignored";
        logger << std::endl;
        logger << "this line should be ignored too";
        logger.log(NdnLoggerLevelInfo) << "this line should be visible" << std::endl;
    }
}


class ThreadLogging {
public:
    ThreadLogging(int n, Logger* logger):
    n_(n),
    logger_(logger),
    logThread_(*webrtc::ThreadWrapper::CreateThread(run, this)),
    isRunning_(false)
    {
    }
    
    void start()
    {
        isRunning_ = true;
        unsigned int tid;
        logThread_.Start(tid);
    }
    void stop()
    {
        isRunning_ = false;
        logThread_.SetNotAlive();
        logThread_.Stop();
    }
    
private:
    webrtc::ThreadWrapper &logThread_;
    int n_;
    bool isRunning_;
    Logger* logger_;
    
    static bool run(void *obj)
    {
        return ((ThreadLogging*)obj)->logSmth();
    }
    bool logSmth()
    {
        logger_->log(NdnLoggerLevelInfo) << "thread " << n_ << " is logging" << std::endl;
        usleep(1000);
        return isRunning_;
    }
    
};

TEST(SimpleLogTests, TestSeveralThreads)
{
    Logger logger;
    ThreadLogging t1(1, &logger);
    ThreadLogging t2(2, &logger);
    
    t1.start();
    t2.start();
    
    WAIT(1000);
    
    t1.stop();
    t2.stop();
}

TEST(SimpleLogTests, TestSeveralThreadsIntoFile)
{
    Logger logger(NdnLoggerDetailLevelAll,
                  ndnrtc::NdnRtcUtils::toString("multi-threaded-2.log"));
    
    ThreadLogging t1(1, &logger);
    ThreadLogging t2(2, &logger);
    
    t1.start();
    t2.start();
    
    WAIT(1000);
    
    t1.stop();
    t2.stop();
}
TEST(SimpleLogTests, TestManyThreadsIntoFile)
{
    Logger logger(NdnLoggerDetailLevelAll,
                  ndnrtc::NdnRtcUtils::toString("multi-threaded-10.log"));
    
    int nThreads = 10;
    ThreadLogging** threads = (ThreadLogging**)malloc(nThreads*sizeof(ThreadLogging*));
    
    for (int i = 0; i < nThreads; i++)
        threads[i] = new ThreadLogging(i, &logger);
    
    for (int i = 0; i < nThreads; i++)
        threads[i]->start();
    
    WAIT(1000);
    
    for (int i = 0; i < nThreads; i++)
        threads[i]->stop();
    
    for (int i = 0; i < nThreads; i++)
        delete threads[i];

    free(threads);
}

TEST(SimpleLogTests, TestLogFactory)
{
    {
        // log to console
        Logger::getLogger("").log(NdnLoggerLevelInfo) << "this should be visible" << std::endl;
        Logger::destroyLogger("");
    }
    {
        // log to file
        Logger::log("factory.log", NdnLoggerLevelInfo) << "test logger factory to file" << std::endl;
        Logger::destroyLogger("factory.log");
    }
    {
        LogTrace("test-macro.log") << "testing macro" << std::endl;
        
        TestLoggingObject obj;
        LogTrace("test-macro.log", &obj) << "macro logging from object" << std::endl;
        
        LogDebug("test-macro.log") << "testing macro debug" << std::endl;
        LogDebug("test-macro.log", &obj) << "testing macro debug from obj" << std::endl;
        
        LogInfo("test-macro.log") << "testing macro info" << std::endl;
        LogInfo("test-macro.log", &obj) << "testing macro info from obj" << std::endl;
        
        LogWarn("test-macro.log") << "testing macro warn" << std::endl;
        LogWarn("test-macro.log", &obj) << "testing macro warn from obj" << std::endl;
        
        LogError("test-macro.log") << "testing macro error" << std::endl;
        LogError("test-macro.log", &obj) << "testing macro error from obj" << std::endl;
        
    }
}