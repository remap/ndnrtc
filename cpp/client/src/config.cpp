//
//  config.cpp
//  ndnrtc
//
//  Copyright (c) 2015 Jiachen Wang. All rights reserved.
//

#include <stdio.h>
#include <libconfig.h++>

#include "config.h"

#define SECTION_GENERAL_KEY "general"
#define CONSUMER_KEY "consume"
#define SECTION_BASIC_KEY "basic"
#define SECTION_AUDIO_KEY "audio"
#define SECTION_VIDEO_KEY "video"
#define SECTION_STREAMS_KEY "streams"
#define CONSUMER_BAISC_STAT_KEY "stat"
#define SECTION_STREAM_TYPE_AUDIO "audio"
#define SECTION_STREAM_TYPE_VIDEO "video"

using namespace std;
using namespace libconfig;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;

int lookupNumber(const Setting &SettingPath,string lookupKey, double &paramToFind);
int lookupNumber(const Setting &SettingPath, string lookupKey, int &paramToFind);
int lookupNumber(const Setting &SettingPath, string lookupKey, unsigned int &paramToFind);
int loadConfigFile(const string &cfgFileName, 
    Config &cfg);
int loadBasicConsumerSettings(const Setting &settingPath, 
    GeneralConsumerParams& consumerGeneralParams);
int loadBasicStatSettings(const Setting &consumerBasicStatSettings, 
    std::vector<Statistics> &statistics);
int loadGeneralSettings(const Setting &general, 
    GeneralParams &generalParams);
int loadMediaStreamParams(const Setting &consumerStreamSettings, 
    MediaStreamParamsSupplement* defaultAudioStream);
int loadAudioThreadParams(const Setting &consumerStreamThreadsSettings, 
    AudioThreadParams* audioThread, MediaStreamParamsSupplement* defaultAudioStream);
int loadVideoThreadParams(const Setting &consumerStreamThreadsSettings, 
    VideoThreadParams* videoThread, MediaStreamParamsSupplement* defaultVideoStream);

//******************************************************************************
int loadParamsFromFile(const string &cfgFileName,
                       ClientParams &params){
    
    Config cfg;

    if (loadConfigFile(cfgFileName, cfg)==EXIT_FAILURE) {
        LogError("") << "loading params from file: "<< cfgFileName << " met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    const Setting &root = cfg.getRoot();
    const Setting &general = root[SECTION_GENERAL_KEY];
    
    if (loadGeneralSettings(general, params.generalParams_)==EXIT_FAILURE) {
        LogError("") << "loading general settings from file: "<< cfgFileName 
            << " met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    const Setting &consumerRootSettings = root[CONSUMER_KEY];

    // setup consumer general settings
    const Setting &consumerBasicSettings = consumerRootSettings[SECTION_BASIC_KEY ];
    const Setting &consumerBasicAudioSettings = consumerBasicSettings[SECTION_AUDIO_KEY];
    const Setting &consumerBasicVideoSettings = consumerBasicSettings[SECTION_VIDEO_KEY];
    const Setting &consumerBasicStatSettings = consumerBasicSettings[CONSUMER_BAISC_STAT_KEY];
    
    if (loadBasicConsumerSettings(consumerBasicAudioSettings, 
            params.audioConsumerParams_)==EXIT_FAILURE) {
        LogError("") << "loading basic consumer audio settings from file: "<< cfgFileName 
            << " met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (loadBasicConsumerSettings(consumerBasicVideoSettings, 
            params.videoConsumerParams_)==EXIT_FAILURE) {
        LogError("") << "loading basic consumer video settings from file: "<< cfgFileName 
            << " met error!" << std::endl;
        return (EXIT_FAILURE);
    }

    if (loadBasicStatSettings(consumerBasicStatSettings,params.statistics_)==EXIT_FAILURE) {
        LogInfo("") << "loading statistic settings settings from file: "<< cfgFileName 
            << " met error, and move on ..." << std::endl;
    }

    try{// setup stream settings
        const Setting &consumerStreamsSettings = consumerRootSettings[SECTION_STREAMS_KEY];
        int streamsNumber=consumerStreamsSettings.getLength();

        for (int streamsCount = 0; streamsCount < streamsNumber; streamsCount++)
        {
            const Setting &consumerStreamSettings=consumerStreamsSettings[streamsCount];
            MediaStreamParamsSupplement* defaultStream = new MediaStreamParamsSupplement(); 
            string streamType="";
            
            consumerStreamSettings.lookupValue("type", streamType);

            if(streamType=="audio") 
            {
                defaultStream->type_ = MediaStreamParams::MediaStreamTypeAudio;

                if (loadMediaStreamParams(consumerStreamSettings, defaultStream)==EXIT_FAILURE) {
                        LogError("") << "loading audio stream[" << streamsCount 
                            << "] settings from file: "<< cfgFileName << " met error!" << std::endl;
                        return (EXIT_FAILURE);
                }
                params.defaultAudioStreams_.push_back(defaultStream);
            }
            else if(streamType=="video") 
            {
                defaultStream->type_ = MediaStreamParams::MediaStreamTypeVideo;

                if (loadMediaStreamParams(consumerStreamSettings, defaultStream)==EXIT_FAILURE) {
                        LogError("") << "loading audio stream[" << streamsCount 
                            << "] settings from file: "<< cfgFileName << " met error!" << std::endl;
                        return (EXIT_FAILURE);
                }
                params.defaultVideoStreams_.push_back(defaultStream);
            }
            else 
            {
                LogError("") << "Unknow stream type!" << std::endl;
                delete defaultStream;

                return(EXIT_FAILURE);
            }          
        }
    }
    catch (const SettingNotFoundException &nfex){
        LogError("") << "Error when loading stream settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    
    return EXIT_SUCCESS;
}

int lookupNumber(const Setting &SettingPath,string lookupKey, double &paramToFind)
{
    unsigned int valueUnsignedInt=0;
    int valueInt=0;
    double valueDouble=0;
    bool valueUnsignedIntState = SettingPath.lookupValue(lookupKey, valueUnsignedInt);
    bool valueIntState = SettingPath.lookupValue(lookupKey, valueInt);
    bool valueDoubleState = SettingPath.lookupValue(lookupKey, valueDouble);

    if(valueUnsignedIntState==true)
    {
        paramToFind=valueUnsignedInt;
    }
    else if (valueIntState==true)
    {
        paramToFind=valueInt;
    }
    else if(valueDoubleState==true)
    {
        paramToFind=valueDouble;
    }
    else
    {
        LogError("") << "Failed to read this param: "<< lookupKey << std::endl;
        return(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

int lookupNumber(const Setting &SettingPath, string lookupKey, int &paramToFind){
    double valueDouble=0;

    lookupNumber(SettingPath, lookupKey, valueDouble);
    paramToFind = (int) valueDouble;
    return EXIT_SUCCESS;
}

int lookupNumber(const Setting &SettingPath, string lookupKey, unsigned int &paramToFind){

    double valueDouble=0;

    lookupNumber(SettingPath, lookupKey, valueDouble);
    paramToFind = (unsigned int) valueDouble;
    return EXIT_SUCCESS;
}

int loadConfigFile(const string &cfgFileName, 
                    Config &cfg){
    
    // Read the file. If there is an error, report it and exit.
    try{
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex){
        LogError("")
        << "I/O error while reading file." << endl;
        return EXIT_FAILURE;
    }
    catch(const ParseException &pex){
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadBasicConsumerSettings(const Setting &settingPath,
                        GeneralConsumerParams& consumerGeneralParams){
    try{
        if (lookupNumber(settingPath, "interest_lifetime", 
                consumerGeneralParams.interestLifetime_)==EXIT_FAILURE) {
            LogError("") << "Lookuping interest_lifetime from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        
        LogTrace("") << "onsumerGeneralParams.interestLifetime_: " 
            << consumerGeneralParams.interestLifetime_ << std::endl;

        if (lookupNumber(settingPath, "jitter_size", 
                consumerGeneralParams.jitterSizeMs_)==EXIT_FAILURE) {
            LogError("") << "Lookuping jitter_size from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }

        if (lookupNumber(settingPath, "buffer_size", 
                consumerGeneralParams.bufferSlotsNum_)==EXIT_FAILURE) {
            LogError("") << "Lookuping buffer_size from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }

        if (lookupNumber(settingPath, "slot_size", consumerGeneralParams.slotSize_)==EXIT_FAILURE) {
            LogError("") << "Lookuping slot_size from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
    }
    catch(const SettingNotFoundException &nfex){
        LogError("") << "Error when loading basic audio Or video settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;

}

int loadBasicStatSettings(const Setting &consumerBasicStatSettings, 
                        std::vector<Statistics> &statistics){
    try{
        int statEntryNumber=consumerBasicStatSettings.getLength();

        LogDebug("") << "total stat entries:  " << statEntryNumber << std::endl;

        for (int statEntryCount = 0; statEntryCount < statEntryNumber; statEntryCount++) {
            Setting &consumerBasicStatEntrySettings = consumerBasicStatSettings[statEntryCount];
            Statistics stat;

            consumerBasicStatEntrySettings.lookupValue("name",stat.statFileName_);
            LogDebug("") << "stat.statisticName: " << stat.statFileName_ << std::endl;
            
            Setting &consumerBasicStatEntryItemSettings = consumerBasicStatEntrySettings["statistics"];
            int statItemStatisticsNumber=consumerBasicStatEntryItemSettings.getLength();
            
            for (int statItemStatisticsCount = 0; statItemStatisticsCount < statItemStatisticsNumber; statItemStatisticsCount++){

                string consumerBasicStatEntryItemSetting=consumerBasicStatEntryItemSettings[statItemStatisticsCount];
                stat.gatheredStatistcs_.push_back(consumerBasicStatEntryItemSetting);
                LogDebug("") << "consumerBasicStatEntryItemSettings[" << statItemStatisticsCount << "]: "<< consumerBasicStatEntryItemSetting << std::endl;
            }
            statistics.push_back(stat);
        }
    }catch (const SettingNotFoundException &nfex){
        LogError("") << "Error when loading statistics settings!" << std::endl;
        // return(EXIT_FAILURE); //can have no statistics
    }
    return EXIT_SUCCESS;
}

int loadGeneralSettings(const Setting &general, 
                        GeneralParams &generalParams){
    try{ 
        // setup general settings
        string log_level, log_file, headlessUser;
        
        if (general.lookupValue("log_level", log_level)){
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
        if (general.lookupValue("log_level", log_level)){
            LogInfo("") << "generalParams.loggingLevel_: " << generalParams.loggingLevel_ << std::endl;
        }

        general.lookupValue("log_file", generalParams.logFile_);
        general.lookupValue("use_tlv", generalParams.useTlv_);
        general.lookupValue("use_rtx", generalParams.useRtx_);
        general.lookupValue("use_fec", generalParams.useFec_);
        general.lookupValue("use_cache", generalParams.useCache_);
        general.lookupValue("use_avsync", generalParams.useAvSync_);
        general.lookupValue("audio", generalParams.useAudio_);
        general.lookupValue("video", generalParams.useVideo_);
        general.lookupValue("skip_incomplete", generalParams.skipIncomplete_);
        
        const Setting &ndnNetwork = general["ndnnetwork"];

        ndnNetwork.lookupValue("connect_host", generalParams.host_);
        LogTrace("") << "generalParams.host_: " << generalParams.host_ << std::endl;
        if (lookupNumber(ndnNetwork, "connect_port", generalParams.portNum_)==EXIT_FAILURE) {
            LogError("") << "Lookup connect_port from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        LogTrace("") << "generalParams.portNum_: " << generalParams.portNum_ << std::endl;
    }
    catch(const SettingNotFoundException &nfex){
        LogError("") << "Error when loading general settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadMediaStreamParams(const Setting &consumerStreamSettings, 
                            MediaStreamParamsSupplement* defaultStream){
    try{
        consumerStreamSettings.lookupValue("session_prefix", defaultStream->streamPrefix_);
        consumerStreamSettings.lookupValue("thread_to_fetch", defaultStream->threadToFetch_);
        LogTrace("") << "thread_to_fetch: " << defaultStream->threadToFetch_ << std::endl;
        consumerStreamSettings.lookupValue("sink", defaultStream->mediaStreamSink_);
        consumerStreamSettings.lookupValue("name", defaultStream->streamName_);
        consumerStreamSettings.lookupValue("segment_size", defaultStream->producerParams_.segmentSize_);

        const Setting &consumerStreamThreadsSettings=consumerStreamSettings["thread"];
        int threadNumber=consumerStreamThreadsSettings.getLength();
        
        for (int threadCount = 0; threadCount < threadNumber; threadCount++){
            //add threads for one audio or video stream
            if (defaultStream->type_ == MediaStreamParams::MediaStreamTypeAudio){
                AudioThreadParams* audioThread=new AudioThreadParams();

                if (loadAudioThreadParams(consumerStreamThreadsSettings[threadCount], audioThread, defaultStream)==EXIT_FAILURE) {
                    LogError("") << "load audioThread settings from file met error!" << std::endl;
                    return (EXIT_FAILURE);
                }
            }
            else if (defaultStream->type_ == MediaStreamParams::MediaStreamTypeVideo){
                VideoThreadParams* videoThread=new VideoThreadParams(); 
                    
                if (loadVideoThreadParams(consumerStreamThreadsSettings[threadCount], videoThread, defaultStream)==EXIT_FAILURE) {
                    LogError("") << "load videoThread settings from file met error!" << std::endl;
                    return (EXIT_FAILURE);
                }
            }
            else {//could only be audio or video
                LogError("") << "Error when loading stream settings: Unknown media thread type!" << std::endl;
                return(EXIT_FAILURE);
            }
        }
    }
    catch(const SettingNotFoundException &nfex){

        LogError("") << "Error when loading media stream basic settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadAudioThreadParams(const Setting &consumerStreamThreadsSettings, 
                            AudioThreadParams* audioThread, 
                            MediaStreamParamsSupplement* defaultAudioStream){
    try{
        consumerStreamThreadsSettings.lookupValue("name", audioThread->threadName_);
        defaultAudioStream->mediaThreads_.push_back(audioThread);
        LogTrace("") << "consumerStreamThreadsSettings(audio): " << audioThread->threadName_ << std::endl;
    }
    catch(const SettingNotFoundException &nfex){
        LogError("") << "Error when loading audio thread settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadVideoThreadParams(const Setting &consumerStreamThreadsSettings, 
                            VideoThreadParams* videoThread, 
                            MediaStreamParamsSupplement* defaultVideoStream){
    try{
        consumerStreamThreadsSettings.lookupValue("name", videoThread->threadName_);
        if (lookupNumber(consumerStreamThreadsSettings, "average_segnum_delta", videoThread->deltaAvgSegNum_)==EXIT_FAILURE) {
            LogError("") << "Lookuping average_segnum_delta from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadsSettings, "average_segnum_delta_parity", videoThread->deltaAvgParitySegNum_)==EXIT_FAILURE) {
            LogError("") << "Lookuping average_segnum_delta_parity from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadsSettings, "average_segnum_key", videoThread->keyAvgSegNum_)==EXIT_FAILURE) {
            LogError("") << "Lookuping average_segnum_key from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadsSettings, "average_segnum_key_parity", videoThread->keyAvgParitySegNum_)==EXIT_FAILURE) {
            LogError("") << "Lookuping average_segnum_key_parity from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        LogTrace("") << "consumerStreamThreadsSettings(video): " << videoThread->keyAvgParitySegNum_ << std::endl;
        
        const Setting &consumerStreamThreadCoderSettings=consumerStreamThreadsSettings["coder"];

        if (lookupNumber(consumerStreamThreadCoderSettings, "frame_rate", videoThread->coderParams_.codecFrameRate_)==EXIT_FAILURE) {
            LogError("") << "Lookuping frame_rate from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadCoderSettings, "gop", videoThread->coderParams_.gop_)==EXIT_FAILURE) {
            LogError("") << "Lookuping gop from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadCoderSettings, "start_bitrate", videoThread->coderParams_.startBitrate_)==EXIT_FAILURE) {
            LogError("") << "Lookuping start_bitrate from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadCoderSettings, "max_bitrate", videoThread->coderParams_.maxBitrate_)==EXIT_FAILURE) {
            LogError("") << "Lookuping max_bitrate from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadCoderSettings, "encode_height", videoThread->coderParams_.encodeWidth_)==EXIT_FAILURE) {
            LogError("") << "Lookuping encode_height from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }
        if (lookupNumber(consumerStreamThreadCoderSettings, "encode_width", videoThread->coderParams_.encodeHeight_)==EXIT_FAILURE) {
            LogError("") << "Lookuping encode_width from file met error!" << std::endl;
            return (EXIT_FAILURE);
        }

        defaultVideoStream->mediaThreads_.push_back(videoThread);
    }
    catch(const SettingNotFoundException &nfex){
        LogError("") << "Error when loading video thread settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}


