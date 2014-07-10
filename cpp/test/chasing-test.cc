//
//  chasing-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 7/7/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "consumer-channel.h"
#include "simple-log.h"
#include "ndnrtc-utils.h"

::testing
::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;
using namespace std;

class FilterLogger : Logger
{
public:
    FilterLogger(const NdnLoggerDetailLevel& logLevel,
                 const std::string& logFile,
                 const std::string& chaseLogFile):Logger(logLevel, logFile), chaseLogger_(NdnLoggerDetailLevelAll, chaseLogFile){}
    
    virtual Logger&
    log(const NdnLogType& logType,
        const ILoggingObject* loggingInstance = nullptr,
        const std::string& locationFile = "",
        const int& locationLine = -1)
    {
        string description = loggingInstance->getDescription();
        
        if (logType == NdnLoggerLevelStat &&
            (description.find("chase-est") != string::npos ||
            description.find("audio-playout") != string::npos))
            return chaseLogger_.log(logType, loggingInstance, locationFile, locationLine);
        
        return Logger::log(logType, loggingInstance, locationFile, locationLine);
    }

private:
    Logger chaseLogger_;
};

TEST(TestChasingAndLatency, TestChasingLatency)
{
    int waitTime = 10;
    char producerC[256];
    string producer = "remap-512";
    int64_t testTimestamp = NdnRtcUtils::millisecondTimestamp();
    string testId = NdnRtcUtils::toString("%ld", testTimestamp);
    string logFileName = NdnRtcUtils::toString("%ld.log", testTimestamp);
    string chaseLogFileName = NdnRtcUtils::toString("%ld-chase.log", testTimestamp);
    
    FILE *params = fopen("params", "r");
    
    if (params)
    {
        memset(producerC, 0, 256);
        fscanf(params, "%d\n", &waitTime);
        fscanf(params, "%s\n", producerC);
        
        producer = string(producerC);
    }
    
    FILE *f = fopen("test.id", "w");
    fwrite(testId.c_str(), testId.length(), 1, f);
    fclose(f);
    
    ParamsStruct videoParams = DefaultParams;
    ParamsStruct audioParams = DefaultParamsAudio;
    
//    videoParams.useAudio = false;
    
    videoParams.segmentSize = 800;
    audioParams.segmentSize = 1054;
    videoParams.setProducerId(producer.c_str());
    audioParams.setProducerId(producer.c_str());
    
    FilterLogger logger(NdnLoggerDetailLevelAll, logFileName, chaseLogFileName);
    ConsumerChannel cc(videoParams, audioParams);
    
    cc.setLogger((Logger*)&logger);
    
    cout << "logging into " << logFileName << endl;
    
    EXPECT_TRUE(RESULT_GOOD(cc.init()));
    EXPECT_TRUE(RESULT_GOOD(cc.startTransmission()));

    WAIT(waitTime*1000);
    
    cc.stopTransmission();
}
