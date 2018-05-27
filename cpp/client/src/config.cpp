//
//  config.cpp
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//

#include <stdio.h>
#include <libconfig.h++>

#include "config.hpp"

#define SECTION_GENERAL_KEY "general"
#define CONSUMER_KEY "consume"
#define PRODUCER_KEY "produce"
#define PRODUCER_FRESHNESS_KEY "freshness"
#define SECTION_BASIC_KEY "basic"
#define SECTION_AUDIO_KEY "audio"
#define SECTION_VIDEO_KEY "video"
#define SECTION_STREAMS_KEY "streams"
#define BASIC_STAT_KEY "stat_gathering"
#define SECTION_STREAM_TYPE_AUDIO "audio"
#define SECTION_STREAM_TYPE_VIDEO "video"

using namespace std;
using namespace libconfig;
using namespace ndnrtc;
using namespace ndnlog;

int lookupNumber(const Setting &SettingPath, string lookupKey,
                 double &paramToFind);
int lookupNumber(const Setting &SettingPath, string lookupKey, int &paramToFind);
int lookupNumber(const Setting &SettingPath, string lookupKey,
                 unsigned int &paramToFind);
int loadConfigFile(const string &cfgFileName, Config &cfg);
int loadBasicConsumerSettings(const Setting &settingPath,
                              GeneralConsumerParams &consumerGeneralParams);
int loadBasicStatSettings(const Setting &consumerBasicStatSettings,
                          std::vector<StatGatheringParams> &statistics);
int loadGeneralSettings(const Setting &general, GeneralParams &generalParams);
int loadConsumerSettings(const Setting &root, ConsumerClientParams &params);
int loadFreshnessSettings(const Setting &producer, 
                          GeneralProducerParams::FreshnessPeriodParams &freshnessParams);
int loadProducerSettings(const Setting &root, ProducerClientParams &params,
                         const std::string &identity);
int loadStreamParams(const Setting &s, ConsumerStreamParams &params);
int loadStreamParams(const Setting &s, ProducerStreamParams &params);
int loadStreamParams(const Setting &s, ClientMediaStreamParams &params);
int loadThreadParams(const Setting &s, AudioThreadParams &params);
int loadThreadParams(const Setting &s, VideoThreadParams &params);

//******************************************************************************
void ProducerStreamParams::getMaxResolution(unsigned int &width,
                                            unsigned int &height) const
{
    if (type_ == MediaStreamTypeAudio)
        return;

    width = 0;
    height = 0;

    for (unsigned int i = 0; i < getThreadNum(); ++i)
    {
        VideoThreadParams *p = getVideoThread(i);

        if (p->coderParams_.encodeWidth_ > width)
        {
            width = p->coderParams_.encodeWidth_;
            height = p->coderParams_.encodeHeight_;
        }
    }
}

//******************************************************************************
int loadParamsFromFile(const string &cfgFileName, ClientParams &params,
                       const std::string &identity)
{
    Config cfg;

    if (loadConfigFile(cfgFileName, cfg) == EXIT_FAILURE)
    {
        LogError("") << "error while loading file: " << cfgFileName << std::endl;
        return 1;
    }

    const Setting &root = cfg.getRoot();
    const Setting &general = root[SECTION_GENERAL_KEY];

    GeneralParams gp;
    if (loadGeneralSettings(general, gp) == EXIT_FAILURE)
    {
        LogError("") << "loading general settings from file: " << cfgFileName
                     << " met error!" << std::endl;
        return 1;
    }
    else
        params.setGeneralParameters(gp);

    ConsumerClientParams consumerParams;
    loadConsumerSettings(root, consumerParams);

    params.setConsumerParams(consumerParams);

    ProducerClientParams producerParams;
    loadProducerSettings(root, producerParams, identity);

    params.setProducerParams(producerParams);

    return 0;
}

int loadConsumerSettings(const Setting &root, ConsumerClientParams &params)
{
    if (!root.exists(CONSUMER_KEY))
    {
        LogInfo("") << "Consumer configuration was not found" << std::endl;
        return EXIT_FAILURE;
    }

    const Setting &consumerRootSettings = root[CONSUMER_KEY];

    try
    {
        // setup consumer general settings
        const Setting &consumerBasicSettings = consumerRootSettings[SECTION_BASIC_KEY];

        bool hasBasicAudio = consumerBasicSettings.exists(SECTION_AUDIO_KEY);

        if (hasBasicAudio)
        {
            const Setting &consumerBasicAudioSettings = consumerBasicSettings[SECTION_AUDIO_KEY];
            hasBasicAudio = (loadBasicConsumerSettings(consumerBasicAudioSettings, params.generalAudioParams_) == EXIT_SUCCESS);
        }

        bool hasBasicVideo = consumerBasicSettings.exists(SECTION_VIDEO_KEY);

        if (hasBasicVideo)
        {
            const Setting &consumerBasicVideoSettings = consumerBasicSettings[SECTION_VIDEO_KEY];
            hasBasicVideo = (loadBasicConsumerSettings(consumerBasicVideoSettings, params.generalVideoParams_) == EXIT_SUCCESS);
        }

        if (consumerBasicSettings.exists(BASIC_STAT_KEY))
        {
            loadBasicStatSettings(consumerBasicSettings[BASIC_STAT_KEY], params.statGatheringParams_);
        }

        try
        { // setup stream settings
            const Setting &streamSettings = consumerRootSettings[SECTION_STREAMS_KEY];

            for (int i = 0; i < streamSettings.getLength(); i++)
            {
                ConsumerStreamParams csp;

                if (loadStreamParams(streamSettings[i], csp) == EXIT_SUCCESS)
                {
                    if (!hasBasicAudio && csp.type_ == ConsumerStreamParams::MediaStreamTypeAudio)
                    {
                        LogWarn("") << "Found audio stream to fetch (" << csp.streamName_
                                    << "), but no basic audio settings were provided. Skipping." << std::endl;
                        continue;
                    }
                    if (!hasBasicVideo && csp.type_ == ConsumerStreamParams::MediaStreamTypeVideo)
                    {
                        LogWarn("") << "Found video stream to fetch (" << csp.streamName_
                                    << "), but no basic video settings were provided. Skipping." << std::endl;
                        continue;
                    }

                    params.fetchedStreams_.push_back(csp);
                }
            } // for i
        }
        catch (const SettingNotFoundException &nfex)
        {
            LogError("") << "Error when loading stream settings!" << std::endl;
            return (EXIT_FAILURE);
        }
    }
    catch (const SettingNotFoundException &e)
    {
        LogError("") << "Setting not found at path: " << e.getPath() << std::endl;
    }
    return EXIT_SUCCESS;
}

int loadProducerSettings(const Setting &root, ProducerClientParams &params, const std::string &identity)
{
    if (!root.exists(PRODUCER_KEY))
    {
        LogInfo("") << "Producer configuration was not found" << std::endl;
        return EXIT_FAILURE;
    }

    const Setting &producerRootSettings = root[PRODUCER_KEY];
    params.prefix_ = identity;

    if (producerRootSettings.exists(BASIC_STAT_KEY))
    {
        loadBasicStatSettings(producerRootSettings[BASIC_STAT_KEY], params.statGatheringParams_);
    }

    try
    { // setup stream settings
        const Setting &streamSettings = producerRootSettings[SECTION_STREAMS_KEY];

        for (int i = 0; i < streamSettings.getLength(); i++)
        {
            ProducerStreamParams psp;

            if (loadStreamParams(streamSettings[i], psp) == EXIT_SUCCESS)
            {
                psp.sessionPrefix_ = identity;
                params.publishedStreams_.push_back(psp);
            }
        } // for i
    }
    catch (const SettingNotFoundException &nfex)
    {
        LogError("") << "Error when loading stream settings!" << std::endl;
        return (EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadConfigFile(const string &cfgFileName, Config &cfg)
{

    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch (const FileIOException &fioex)
    {
        LogError("")
            << "I/O error while reading file." << endl;
        return EXIT_FAILURE;
    }
    catch (const ParseException &pex)
    {
        LogError("")
            << "Parse error at " << pex.getFile()
            << ":" << pex.getLine()
            << " - " << pex.getError() << endl;
        return (EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadBasicConsumerSettings(const Setting &s, GeneralConsumerParams &gcp)
{
    if (lookupNumber(s, "interest_lifetime", gcp.interestLifetime_) == EXIT_FAILURE)
    {
        LogError("") << "Reading interest_lifetime from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (lookupNumber(s, "jitter_size", gcp.jitterSizeMs_) == EXIT_FAILURE)
    {
        LogError("") << "Reading jitter_size from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int loadBasicStatSettings(const Setting &s, std::vector<StatGatheringParams> &stats)
{
    try
    {
        LogDebug("") << "total stat entries:  " << s.getLength() << std::endl;

        for (int i = 0; i < s.getLength(); i++)
        {
            Setting &ss = s[i];
            StatGatheringParams stat;

            ss.lookupValue("name", stat.statFileName_);
            LogDebug("") << "stat.statisticName: " << stat.statFileName_ << std::endl;

            Setting &statKeywords = ss["statistics"];
            for (int j = 0; j < statKeywords.getLength(); j++)
            {

                string statKeyword = statKeywords[j];
                stat.addStat(statKeyword);
            } // for j

            stats.push_back(stat);
        } // for i
    }
    catch (const SettingNotFoundException &nfex)
    {
        LogInfo("") << "Statistics settings were not found at path: " << nfex.getPath() << std::endl;
    }

    return EXIT_SUCCESS;
}

int loadGeneralSettings(const Setting &general, GeneralParams &generalParams)
{
    try
    {
        // setup general settings
        string log_level, log_file, headlessUser;

        if (general.lookupValue("log_level", log_level))
        {
            if (log_level == "default")
                generalParams.loggingLevel_ = NdnLoggerDetailLevelDefault;
            if (log_level == "debug")
                generalParams.loggingLevel_ = NdnLoggerDetailLevelDebug;
            if (log_level == "stat")
                generalParams.loggingLevel_ = NdnLoggerDetailLevelStat;
            if (log_level == "all")
                generalParams.loggingLevel_ = NdnLoggerDetailLevelAll;
            if (log_level == "none")
                generalParams.loggingLevel_ = NdnLoggerDetailLevelNone;
        }
        if (general.lookupValue("log_level", log_level))
        {
            LogTrace("") << "generalParams.loggingLevel_: " << generalParams.loggingLevel_ << std::endl;
        }

        general.lookupValue("log_file", generalParams.logFile_);
        general.lookupValue("log_path", generalParams.logPath_);
        general.lookupValue("use_fec", generalParams.useFec_);
        general.lookupValue("use_avsync", generalParams.useAvSync_);

        const Setting &ndnNetwork = general["ndnnetwork"];

        ndnNetwork.lookupValue("connect_host", generalParams.host_);
        LogTrace("") << "generalParams.host_: " << generalParams.host_ << std::endl;
        if (lookupNumber(ndnNetwork, "connect_port", generalParams.portNum_) == EXIT_FAILURE)
        {
            LogError("") << "Lookup connect_port from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        LogTrace("") << "generalParams.portNum_: " << generalParams.portNum_ << std::endl;
    }
    catch (const SettingNotFoundException &nfex)
    {
        LogError("") << "Error when loading setting at path: " << nfex.getPath() << std::endl;
        return (EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadStreamParams(const Setting &s, ConsumerStreamParams &params)
{
    // ClientMediaStreamParams msp;
    if (EXIT_SUCCESS == loadStreamParams(s, (ClientMediaStreamParams &)params))
    {
        s.lookupValue("thread_to_fetch", params.threadToFetch_);

        const Setting &sinkSettings = s["sink"];

        sinkSettings.lookupValue("name", params.sink_.name_);
        
        params.sink_.writeFrameInfo_ = false;
        sinkSettings.lookupValue("write_frame_info", params.sink_.writeFrameInfo_);

        params.sink_.type_ = "file";
        sinkSettings.lookupValue("type", params.sink_.type_);

        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

int loadStreamParams(const Setting &s, ProducerStreamParams &params)
{
    // ClientMediaStreamParams msp;
    if (EXIT_SUCCESS == loadStreamParams(s, (ClientMediaStreamParams &)params))
    {
        s.lookupValue("source", params.source_);

        if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
        {
            string threadName;
            s.lookupValue("thread", threadName);
            params.addMediaThread(AudioThreadParams(threadName));
        }

        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

int loadStreamParams(const Setting &s, ClientMediaStreamParams &params)
{
    try
    {
        string streamType;
        s.lookupValue("type", streamType);

        if (streamType == "video")
            params.type_ = MediaStreamParams::MediaStreamTypeVideo;
        else if (streamType == "audio")
            params.type_ = MediaStreamParams::MediaStreamTypeAudio;
        else
        {
            LogError("") << "Unknown media stream type: " << streamType << std::endl;
            return EXIT_FAILURE;
        }

        s.lookupValue("name", params.streamName_);             // both
        s.lookupValue("sync", params.synchronizedStreamName_); // both

        s.lookupValue("base_prefix", params.sessionPrefix_);                // consumer
        s.lookupValue("segment_size", params.producerParams_.segmentSize_); // producer

        try 
        {
            if (loadFreshnessSettings(s, params.producerParams_.freshness_) == EXIT_FAILURE)
                LogError("") << "couldn't load freshness parameters for producer" << std::endl;
        }
        catch (const SettingNotFoundException &nfex)
        {
            // might've been consumer settings. continue
        }

        try
        { // audio streams do not have thread configurations
            const Setting &threadSettings = s["threads"];

            for (int i = 0; i < threadSettings.getLength(); i++)
            {
                //add threads for one audio or video stream
                if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
                {
                    AudioThreadParams audioThread;

                    if (loadThreadParams(threadSettings[i], audioThread) == EXIT_FAILURE)
                    {
                        LogError("") << "load audioThread settings from file met error!" << std::endl;
                        return (EXIT_FAILURE);
                    }

                    params.addMediaThread(audioThread);
                }
                else
                {
                    VideoThreadParams videoThread;

                    if (loadThreadParams(threadSettings[i], videoThread) == EXIT_FAILURE)
                    {
                        LogError("") << "load videoThread settings from file met error!" << std::endl;
                        return (EXIT_FAILURE);
                    }

                    params.addMediaThread(videoThread);
                }
            } // for i
        }
        catch (const SettingNotFoundException &nfex)
        {
            s.lookupValue("capture_device", params.captureDevice_.deviceId_);
        }
    }
    catch (const SettingNotFoundException &nfex)
    {
        cout << "check " << nfex.getPath() << std::endl;
        LogError("") << "Setting not found at path: " << nfex.getPath() << std::endl;
        return (EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int loadThreadParams(const Setting &s, AudioThreadParams &params)
{
    return s.lookupValue("name", params.threadName_);
}

int loadFreshnessSettings(const Setting &s, 
                          GeneralProducerParams::FreshnessPeriodParams &params)
{
    const Setting &freshnessSettings = s[PRODUCER_FRESHNESS_KEY];
    freshnessSettings.lookupValue("metadata", params.metadataMs_);
    freshnessSettings.lookupValue("sample", params.sampleMs_);
    freshnessSettings.lookupValue("sampleKey", params.sampleKeyMs_);
}

int loadThreadParams(const Setting &s, VideoThreadParams &params)
{

    s.lookupValue("name", params.threadName_);
    lookupNumber(s, "average_segnum_delta", params.segInfo_.deltaAvgSegNum_);
    lookupNumber(s, "average_segnum_delta_parity", params.segInfo_.deltaAvgParitySegNum_);
    lookupNumber(s, "average_segnum_key", params.segInfo_.keyAvgSegNum_);
    lookupNumber(s, "average_segnum_key_parity", params.segInfo_.keyAvgParitySegNum_);

    const Setting &coderSettings = s["coder"];

    if (lookupNumber(coderSettings, "frame_rate", params.coderParams_.codecFrameRate_) == EXIT_FAILURE)
    {
        LogError("") << "Lookuping frame_rate from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (lookupNumber(coderSettings, "gop", params.coderParams_.gop_) == EXIT_FAILURE)
    {
        LogError("") << "Lookuping gop from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (lookupNumber(coderSettings, "start_bitrate", params.coderParams_.startBitrate_) == EXIT_FAILURE)
    {
        LogError("") << "Lookuping start_bitrate from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (lookupNumber(coderSettings, "max_bitrate", params.coderParams_.maxBitrate_) == EXIT_FAILURE)
    {
        LogError("") << "Lookuping max_bitrate from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (lookupNumber(coderSettings, "encode_height", params.coderParams_.encodeHeight_) == EXIT_FAILURE)
    {
        LogError("") << "Lookuping encode_height from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (lookupNumber(coderSettings, "encode_width", params.coderParams_.encodeWidth_) == EXIT_FAILURE)
    {
        LogError("") << "Lookuping encode_width from file met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    coderSettings.lookupValue("drop_frames", params.coderParams_.dropFramesOn_);

    return EXIT_SUCCESS;
}

int lookupNumber(const Setting &SettingPath, string lookupKey, double &paramToFind)
{
    unsigned int valueUnsignedInt = 0;
    int valueInt = 0;
    double valueDouble = 0;
    bool valueUnsignedIntState = SettingPath.lookupValue(lookupKey, valueUnsignedInt);
    bool valueIntState = SettingPath.lookupValue(lookupKey, valueInt);
    bool valueDoubleState = SettingPath.lookupValue(lookupKey, valueDouble);

    if (valueUnsignedIntState == true)
    {
        paramToFind = valueUnsignedInt;
    }
    else if (valueIntState == true)
    {
        paramToFind = valueInt;
    }
    else if (valueDoubleState == true)
    {
        paramToFind = valueDouble;
    }
    else
    {
        return (EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int lookupNumber(const Setting &SettingPath, string lookupKey, int &paramToFind)
{
    double valueDouble = 0;

    lookupNumber(SettingPath, lookupKey, valueDouble);
    paramToFind = (int)valueDouble;
    return EXIT_SUCCESS;
}

int lookupNumber(const Setting &SettingPath, string lookupKey, unsigned int &paramToFind)
{

    double valueDouble = 0;

    lookupNumber(SettingPath, lookupKey, valueDouble);
    paramToFind = (unsigned int)valueDouble;
    return EXIT_SUCCESS;
}