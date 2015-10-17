//
//  config.c
//  ndnrtc
//
//  Created by Peter Gusev on 10/10/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
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


int getValueIntOrDoubleOrUnsignedInt(const Setting &SettingPath,string lookupKey, double &paramToFind){

    unsigned int valueUnsignedInt=0;
    int valueInt=0;
    double valueDouble=0;
    bool valueUnsignedIntState = SettingPath.lookupValue(lookupKey, valueUnsignedInt);
    bool valueIntState = SettingPath.lookupValue(lookupKey, valueInt);
    bool valueDoubleState = SettingPath.lookupValue(lookupKey, valueDouble);

    if(valueUnsignedIntState==true){
        paramToFind=valueUnsignedInt;
    }else if (valueIntState==true){
        paramToFind=valueInt;
    }
    else if(valueDoubleState==true){
        paramToFind=valueDouble;
    }
    else{
        LogError("") << "Failed to read this param: "<< lookupKey << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int getValueIntOrDoubleOrUnsignedInt(const Setting &SettingPath, string lookupKey, int &paramToFind){

    double valueDouble=0;

    getValueIntOrDoubleOrUnsignedInt(SettingPath, lookupKey, valueDouble);
    paramToFind = (int) valueDouble;
    return EXIT_SUCCESS;
}
int getValueIntOrDoubleOrUnsignedInt(const Setting &SettingPath, string lookupKey, unsigned int &paramToFind){

    double valueDouble=0;

    getValueIntOrDoubleOrUnsignedInt(SettingPath, lookupKey, valueDouble);
    paramToFind = (unsigned int) valueDouble;
    return EXIT_SUCCESS;
}

int loadConfigFile(const string &cfgFileName, Config &cfg){
    
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
}
int loadBasicAudioOrVideoSettings(const Setting &settingPath,
                        GeneralConsumerParams& consumerGeneralParams){
    try{
        getValueIntOrDoubleOrUnsignedInt(settingPath, "interest_lifetime", consumerGeneralParams.interestLifetime_);
        LogTrace("") << "onsumerGeneralParams.interestLifetime_: " << consumerGeneralParams.interestLifetime_ << std::endl;
        getValueIntOrDoubleOrUnsignedInt(settingPath, "jitter_size", consumerGeneralParams.jitterSizeMs_);
        getValueIntOrDoubleOrUnsignedInt(settingPath, "buffer_size", consumerGeneralParams.bufferSlotsNum_);
        getValueIntOrDoubleOrUnsignedInt(settingPath, "slot_size", consumerGeneralParams.slotSize_);
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
            Statistics* stat=new Statistics();

            consumerBasicStatEntrySettings.lookupValue("name",stat->statFileName_);
            LogDebug("") << "stat.statisticName: " << stat->statFileName_ << std::endl;
            
            Setting &consumerBasicStatEntryItemSettings = consumerBasicStatEntrySettings["statistics"];
            int statItemStatisticsNumber=consumerBasicStatEntryItemSettings.getLength();
            
            for (int statItemStatisticsCount = 0; statItemStatisticsCount < statItemStatisticsNumber; statItemStatisticsCount++){

                string consumerBasicStatEntryItemSetting=consumerBasicStatEntryItemSettings[statItemStatisticsCount];
                stat->gatheredStatistcs_.push_back(consumerBasicStatEntryItemSetting);
                LogDebug("") << "consumerBasicStatEntryItemSettings[" << statItemStatisticsCount << "]: "<< consumerBasicStatEntryItemSetting << std::endl;
            }
            statistics.push_back(*stat);
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
        getValueIntOrDoubleOrUnsignedInt(ndnNetwork, "connect_port", generalParams.portNum_);
        LogTrace("") << "generalParams.portNum_: " << generalParams.portNum_ << std::endl;
    }
    catch(const SettingNotFoundException &nfex){
        LogError("") << "Error when loading general settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadMediaStreamParams(const Setting &consumerStreamSettings, 
                            MediaStreamParamsSupplement* &defaultAudioStream){
    try{
        consumerStreamSettings.lookupValue("session_prefix", defaultAudioStream->streamPrefix_);
        consumerStreamSettings.lookupValue("thread_to_fetch", defaultAudioStream->threadToFetch_);
        LogTrace("") << "thread_to_fetch: " << defaultAudioStream->threadToFetch_ << std::endl;
        consumerStreamSettings.lookupValue("sink", defaultAudioStream->mediaStreamSink_);
        consumerStreamSettings.lookupValue("name", defaultAudioStream->streamName_);
        consumerStreamSettings.lookupValue("segment_size", defaultAudioStream->producerParams_.segmentSize_);
        }
    catch(const SettingNotFoundException &nfex){

        LogError("") << "Error when loading meadia stream basic settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadAudioThreadParams(const Setting &consumerStreamThreadsSettings, 
                            AudioThreadParams* &audioThread, 
                            MediaStreamParamsSupplement* &defaultAudioStream){
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
                            VideoThreadParams* &videoThread, 
                            MediaStreamParamsSupplement* &defaultVideoStream){
    try{
        consumerStreamThreadsSettings.lookupValue("name", videoThread->threadName_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadsSettings, "average_segnum_delta", videoThread->deltaAvgSegNum_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadsSettings, "average_segnum_delta_parity", videoThread->deltaAvgParitySegNum_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadsSettings, "average_segnum_key", videoThread->keyAvgSegNum_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadsSettings, "average_segnum_key_parity", videoThread->keyAvgParitySegNum_);
        LogTrace("") << "consumerStreamThreadsSettings(video): " << videoThread->keyAvgParitySegNum_ << std::endl;
        
        const Setting &consumerStreamThreadCoderSettings=consumerStreamThreadsSettings["coder"];

        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadCoderSettings, "frame_rate", videoThread->coderParams_.codecFrameRate_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadCoderSettings, "gop", videoThread->coderParams_.gop_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadCoderSettings, "start_bitrate", videoThread->coderParams_.startBitrate_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadCoderSettings, "max_bitrate", videoThread->coderParams_.maxBitrate_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadCoderSettings, "encode_height", videoThread->coderParams_.encodeWidth_);
        getValueIntOrDoubleOrUnsignedInt(consumerStreamThreadCoderSettings, "encode_width", videoThread->coderParams_.encodeHeight_);

        defaultVideoStream->mediaThreads_.push_back(videoThread);
    }
    catch(const SettingNotFoundException &nfex){
        LogError("") << "Error when loading video thread settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int loadParamsFromFile(const string &cfgFileName,
                       ClientParams &params){
    
    Config cfg;
    
    loadConfigFile(cfgFileName, cfg);

    const Setting &root = cfg.getRoot();
    const Setting &general = root[SECTION_GENERAL_KEY];
    
    loadGeneralSettings(general, params.generalParams_);

    const Setting &consumerRootSettings = root[CONSUMER_KEY];

    // setup consumer general settings
    const Setting &consumerBasicSettings = consumerRootSettings[SECTION_BASIC_KEY ];
    const Setting &consumerBasicAudioSettings = consumerBasicSettings[SECTION_AUDIO_KEY];
    const Setting &consumerBasicVideoSettings = consumerBasicSettings[SECTION_VIDEO_KEY];
    const Setting &consumerBasicStatSettings = consumerBasicSettings[CONSUMER_BAISC_STAT_KEY];
    
    loadBasicAudioOrVideoSettings(consumerBasicAudioSettings, params.audioConsumerParams_);
    loadBasicAudioOrVideoSettings(consumerBasicVideoSettings, params.videoConsumerParams_);
    loadBasicStatSettings(consumerBasicStatSettings,params.statistics_);

    
    try{// setup stream settings
        const Setting &consumerStreamsSettings = consumerRootSettings[SECTION_STREAMS_KEY];
        int streamsNumber=consumerStreamsSettings.getLength();

        for (int streamsCount = 0; streamsCount < streamsNumber; streamsCount++){

            const Setting &consumerStreamSettings=consumerStreamsSettings[streamsCount];
            MediaStreamParamsSupplement* defaultAudioStream = new MediaStreamParamsSupplement(); 
            string streamType="";
            
            consumerStreamSettings.lookupValue("type", streamType);

            if(streamType=="audio"){
                defaultAudioStream->type_ = MediaStreamParams::MediaStreamTypeAudio;
                loadMediaStreamParams(consumerStreamSettings,defaultAudioStream);

                const Setting &consumerStreamThreadsSettings=consumerStreamSettings["thread"];
                int audioThreadNumber=consumerStreamThreadsSettings.getLength();
                
                for (int audioThreadCount = 0; audioThreadCount < audioThreadNumber; audioThreadCount++){

                    AudioThreadParams* audioThread=new AudioThreadParams();

                    loadAudioThreadParams(consumerStreamThreadsSettings[audioThreadCount], audioThread, defaultAudioStream);
                }
                params.defaultAudioStreams_.push_back(defaultAudioStream);
            }
            else if(streamType=="video"){
                MediaStreamParamsSupplement* defaultVideoStream= new MediaStreamParamsSupplement();

                defaultVideoStream->type_ = MediaStreamParams::MediaStreamTypeVideo;
                loadMediaStreamParams(consumerStreamSettings,defaultVideoStream);

                const Setting &consumerStreamThreadsSettings=consumerStreamSettings["thread"];
                int videoThreadNumber=consumerStreamThreadsSettings.getLength();
                
                for (int videoThreadCount = 0; videoThreadCount < videoThreadNumber; videoThreadCount++){
                    VideoThreadParams* videoThread=new VideoThreadParams(); 
                    
                    loadVideoThreadParams(consumerStreamThreadsSettings[videoThreadCount], videoThread, defaultVideoStream);
                }
                params.defaultVideoStreams_.push_back(defaultVideoStream);
            }else{
                LogError("") << "Unknow stream type!" << std::endl;
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
