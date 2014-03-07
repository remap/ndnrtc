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
        new_api::Logger logger;
        
        logger.log(NdnLoggerLevelInfo)
            << "you should be able to see this line in the console.";
        logger << " adding number: " << 1234;
        logger << ". ending line" << endl;
        
        logger.log(NdnLoggerLevelInfo) << "this is a new logging line and it ends here" << endl;
        logger.log(NdnLoggerLevelInfo) << "yet another new log line" << endl;
    }
    { // log with levels
        new_api::Logger logger;
        
        logger.log(NdnLoggerLevelTrace) << "trace level";
        logger.log(NdnLoggerLevelDebug) << "debug level";
        logger.log(NdnLoggerLevelInfo) << "info level";
        logger.log(NdnLoggerLevelWarning) << "warning level";
        logger.log(NdnLoggerLevelError) << "error level" << endl;
    }
    { // log with default level
        new_api::Logger logger(NdnLoggerDetailLevelDefault);
        
        logger.log(NdnLoggerLevelTrace) << "trace level - should be ignored";
        logger.log(NdnLoggerLevelDebug) << "debug level - should be ignored";
        logger.log(NdnLoggerLevelInfo) << "info level";
        logger.log(NdnLoggerLevelWarning) << "warning level";
        logger.log(NdnLoggerLevelError) << "error level" << endl;
    }
    { // logger level none
        new_api::Logger logger(NdnLoggerDetailLevelNone);
        
        logger.log(NdnLoggerLevelTrace) << "trace level - should be ignored";
        logger.log(NdnLoggerLevelDebug) << "debug level - should be ignored";
        logger.log(NdnLoggerLevelInfo) << "info level - should be ignored";
        logger.log(NdnLoggerLevelWarning) << "warning level - should be ignored";
        logger.log(NdnLoggerLevelError) << "error level - should be ignored";
    }
    { // test trace and debug
        new_api::Logger logger;
        
        logger.log(NdnLoggerLevelTrace, nullptr, __FILE__, __LINE__) << "trace entry at this file and line";
        logger.log(NdnLoggerLevelDebug, nullptr, __FILE__, __LINE__) << "no info on sources for debug level" << endl;
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
        new_api::Logger logger;
        
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
        
        new_api::Logger logger(NdnLoggerDetailLevelAll, fileName);
        
        EXPECT_TRUE(UnitTestHelper::checkFileExists(fileName.c_str()));
        
        logger.log(NdnLoggerLevelInfo) << "test log to file " << 1234;
        
        TestLoggingObject obj;
        logger.log(NdnLoggerLevelInfo, &obj) << "log from object" << endl;
    }
}

TEST(SimpleLogTests, TestLogInvalid)
{
    {
        new_api::Logger logger;
        
        logger << "this line should be ignored";
        logger << endl;
        logger << "this line should be ignored too";
        logger.log(NdnLoggerLevelInfo) << "this line should be visible" << endl;
    }
}


class ThreadLogging {
public:
    ThreadLogging(int n, new_api::Logger* logger):
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
    new_api::Logger* logger_;
    
    static bool run(void *obj)
    {
        return ((ThreadLogging*)obj)->logSmth();
    }
    bool logSmth()
    {
        logger_->log(NdnLoggerLevelInfo) << "thread " << n_ << " is logging" << endl;
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
        Logger::getLogger("").log(NdnLoggerLevelInfo) << "this should be visible" << endl;
        Logger::destroyLogger("");
    }
    {
        // log to file
        Logger::log("factory.log", NdnLoggerLevelInfo) << "test logger factory to file" << endl;
        Logger::destroyLogger("factory.log");
    }
    {
        LogTrace("test-macro.log") << "testing macro" << endl;
        
        TestLoggingObject obj;
        LogTrace("test-macro.log", &obj) << "macro logging from object" << endl;
        
        LogDebug("test-macro.log") << "testing macro debug" << endl;
        LogDebug("test-macro.log", &obj) << "testing macro debug from obj" << endl;
        
        LogInfo("test-macro.log") << "testing macro info" << endl;
        LogInfo("test-macro.log", &obj) << "testing macro info from obj" << endl;
        
        LogWarn("test-macro.log") << "testing macro warn" << endl;
        LogWarn("test-macro.log", &obj) << "testing macro warn from obj" << endl;
        
        LogError("test-macro.log") << "testing macro error" << endl;
        LogError("test-macro.log", &obj) << "testing macro error from obj" << endl;
        
    }
}