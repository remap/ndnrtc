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

#define GENERAL_KEY "general"
#define CONSUMER_KEY "consumer"
#define CONSUMER_BAISC_KEY "basic"
#define CONSUMER_BAISC_AUDIO_KEY "audio"
#define CONSUMER_BAISC_VIDEO_KEY "video"
#define CONSUMER_BAISC_STAT_KEY "stat"
#define CONSUMER_STREAMS_KEY "streams"

using namespace std;
using namespace libconfig;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnlog;


int getValueIntOrDouble(const Setting &SettingPath,string lookupKey, double &paramToFind){
    int valueInt=0;
    double valueDouble=0;
    bool valueIntState = SettingPath.lookupValue(lookupKey, valueInt);
    bool valueDoubleState = SettingPath.lookupValue(lookupKey, valueDouble);

    if (valueIntState==true){
        paramToFind=valueInt;
    }
    else if(valueDoubleState==true){
        paramToFind=valueDouble;
    }
    else{
        LogInfo("") << "Failed to read this param: "<< lookupKey << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int getValueIntOrDouble(const Setting &SettingPath, string lookupKey, int &paramToFind){
    double valueDouble=0;

    getValueIntOrDouble(SettingPath, lookupKey, valueDouble);
    paramToFind = (int) valueDouble;
    return EXIT_SUCCESS;
}

int loadConfigFile(const string &cfgFileName, Config &cfg){
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return EXIT_FAILURE;
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
}

int loadParamsFromFile(const string &cfgFileName,
                       ClientParams &params){
    
    Config cfg;
    loadConfigFile(cfgFileName, cfg);
    const Setting &root = cfg.getRoot();
    
    try{
        root.lookupValue("app_online_time", params.headlessAppOnlineTime);
        LogInfo("") << "app_online_time(seconds): " << params.headlessAppOnlineTime << std::endl;

    }catch(const SettingNotFoundException &nfex){
        // ignore
    }

    try
    { // setup general settings
        const Setting &general = root[GENERAL_KEY];
        string log_level, log_file, headlessUser;
        
        if (general.lookupValue("log_level", log_level))
        {
            if (log_level == "default")
                params.generalParams_.loggingLevel_ = NdnLoggerDetailLevelDefault;
            if (log_level == "debug")
                params.generalParams_.loggingLevel_ = NdnLoggerDetailLevelDebug;
            if (log_level == "all")
                params.generalParams_.loggingLevel_ = NdnLoggerDetailLevelAll;
            if (log_level == "none")
                params.generalParams_.loggingLevel_ = NdnLoggerDetailLevelNone;
        }
        if (general.lookupValue("log_level", log_level)){
            LogInfo("") << "params.generalParams_.loggingLevel_: " << params.generalParams_.loggingLevel_ << std::endl;
        }

        general.lookupValue("log_file", params.generalParams_.logFile_);
        general.lookupValue("use_tlv", params.generalParams_.useTlv_);
        general.lookupValue("use_rtx", params.generalParams_.useRtx_);
        general.lookupValue("use_fec", params.generalParams_.useFec_);
        general.lookupValue("use_cache", params.generalParams_.useCache_);
        general.lookupValue("use_avsync", params.generalParams_.useAvSync_);
        general.lookupValue("audio", params.generalParams_.useAudio_);
        general.lookupValue("video", params.generalParams_.useVideo_);
        general.lookupValue("skip_incomplete", params.generalParams_.skipIncomplete_);
        
        const Setting &ndnNetwork = general["ndnnetwork"];
        ndnNetwork.lookupValue("connect_host", params.generalParams_.host_);
        LogInfo("") << "params.generalParams_.host_: " << params.generalParams_.host_ << std::endl;
        ndnNetwork.lookupValue("connect_port", params.generalParams_.portNum_);
        LogInfo("") << "params.generalParams_.portNum_: " << params.generalParams_.portNum_ << std::endl;
    }
    catch(const SettingNotFoundException &nfex)
    {
        LogInfo("") << "Error when loading general settings!" << std::endl;
    }

    const Setting &consumerRootSettings = root[CONSUMER_KEY];

    try
    {// setup consumer general settings
        const Setting &consumerBasicSettings = consumerRootSettings[CONSUMER_BAISC_KEY];
        const Setting &consumerBasicAudioSettings = consumerBasicSettings[CONSUMER_BAISC_AUDIO_KEY];
        const Setting &consumerBasicVideoSettings = consumerBasicSettings[CONSUMER_BAISC_VIDEO_KEY];
        const Setting &consumerBasicStatSettings = consumerBasicSettings[CONSUMER_BAISC_STAT_KEY];
        
        consumerBasicAudioSettings.lookupValue("interest_lifetime", params.audioConsumerParams_.interestLifetime_);
        LogInfo("") << "params.audioConsumerParams_.interestLifetime_: " << params.audioConsumerParams_.interestLifetime_ << std::endl;
        consumerBasicAudioSettings.lookupValue("jitter_size", params.audioConsumerParams_.jitterSizeMs_);
        consumerBasicAudioSettings.lookupValue("buffer_size", params.audioConsumerParams_.bufferSlotsNum_);
        consumerBasicAudioSettings.lookupValue("slot_size", params.audioConsumerParams_.slotSize_);
        
        consumerBasicVideoSettings.lookupValue("interest_lifetime", params.videoConsumerParams_.interestLifetime_);
        LogInfo("") << "params.videoConsumerParams_.interestLifetime_: " << params.videoConsumerParams_.interestLifetime_ << std::endl;
        consumerBasicVideoSettings.lookupValue("jitter_size", params.videoConsumerParams_.jitterSizeMs_);
        consumerBasicVideoSettings.lookupValue("buffer_size", params.videoConsumerParams_.bufferSlotsNum_);
        consumerBasicVideoSettings.lookupValue("slot_size", params.videoConsumerParams_.slotSize_);

        int statEntryNumber=consumerBasicStatSettings.getLength();
        LogInfo("") << "statEntryNumber: " << statEntryNumber << std::endl;
        for (int statEntryCount = 0; statEntryCount < statEntryNumber; statEntryCount++){
            Setting &consumerBasicStatEntrySettings = consumerBasicSettings[CONSUMER_BAISC_STAT_KEY][statEntryCount];
            Statistics* stat=new Statistics();
            consumerBasicStatEntrySettings.lookupValue("name",stat->statisticName);
            LogInfo("") << "stat.statisticName: " << stat->statisticName << std::endl;
            Setting &consumerBasicStatEntryItemSettings = consumerBasicStatEntrySettings["statistics"];
            int statItemStatisticsNumber=consumerBasicStatEntryItemSettings.getLength();
            for (int statItemStatisticsCount = 0; statItemStatisticsCount < statItemStatisticsNumber; statItemStatisticsCount++){
                string consumerBasicStatEntryItemSetting=consumerBasicStatEntryItemSettings[statItemStatisticsCount];
                stat->statisticItem.push_back(consumerBasicStatEntryItemSetting);
                LogInfo("") << "consumerBasicStatEntryItemSettings[" << statItemStatisticsCount << "]: "<< consumerBasicStatEntryItemSetting << std::endl;
            }
            params.statistics_.push_back(*stat);
        }
        
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
    }

    
    try
    {// setup stream settings
        const Setting &consumerStreamsSettings = consumerRootSettings[CONSUMER_STREAMS_KEY];
        int streamsNumber=consumerStreamsSettings.getLength();
        for (int streamsCount = 0; streamsCount < streamsNumber; streamsCount++){

            const Setting &consumerStreamSettings=consumerStreamsSettings[streamsCount];
            string streamType="";
            consumerStreamSettings.lookupValue("type", streamType);
            if(streamType=="audio"){
                MediaStreamParamsSupplement defaultAudioStream; 
                defaultAudioStream.mediaStream_.type_ = MediaStreamParams::MediaStreamTypeAudio;
                consumerStreamSettings.lookupValue("session_prefix", defaultAudioStream.streamPrefix);
                consumerStreamSettings.lookupValue("threadToFetch", defaultAudioStream.threadToFetch);
                consumerStreamSettings.lookupValue("sink", defaultAudioStream.mediaStreamSink_);
                consumerStreamSettings.lookupValue("name", defaultAudioStream.mediaStream_.streamName_);
                consumerStreamSettings.lookupValue("segment_size", defaultAudioStream.mediaStream_.producerParams_.segmentSize_);

                const Setting &consumerStreamThreadsSettings=consumerStreamSettings["thread"];
                int audioThreadNumber=consumerStreamThreadsSettings.getLength();
                
                for (int audioThreadCount = 0; audioThreadCount < audioThreadNumber; audioThreadCount++){
                    AudioThreadParams* audioThread=new AudioThreadParams();
                    consumerStreamThreadsSettings[audioThreadCount].lookupValue("name", audioThread->threadName_);
                    defaultAudioStream.mediaStream_.mediaThreads_.push_back(audioThread);
                    LogInfo("") << "consumerStreamThreadsSettings[" << audioThreadCount << "](audio): " << audioThread->threadName_ << std::endl;
                }
                 params.defaultAudioStreams_.push_back(defaultAudioStream);
            }else if(streamType=="video"){
                MediaStreamParamsSupplement defaultVideoStream;
                defaultVideoStream.mediaStream_.type_ = MediaStreamParams::MediaStreamTypeVideo;
                consumerStreamSettings.lookupValue("session_prefix", defaultVideoStream.streamPrefix);
                consumerStreamSettings.lookupValue("threadToFetch", defaultVideoStream.threadToFetch);
                consumerStreamSettings.lookupValue("sink", defaultVideoStream.mediaStreamSink_);
                consumerStreamSettings.lookupValue("name", defaultVideoStream.mediaStream_.streamName_);
                consumerStreamSettings.lookupValue("segment_size", defaultVideoStream.mediaStream_.producerParams_.segmentSize_);

                const Setting &consumerStreamThreadsSettings=consumerStreamSettings["thread"];
                int videoThreadNumber=consumerStreamThreadsSettings.getLength();
                
                for (int videoThreadCount = 0; videoThreadCount < videoThreadNumber; videoThreadCount++){
                    VideoThreadParams* videoThread=new VideoThreadParams(); 
                    consumerStreamThreadsSettings[videoThreadCount].lookupValue("name", videoThread->threadName_);
                    getValueIntOrDouble(consumerStreamThreadsSettings[videoThreadCount], "average_segnum_delta", videoThread->deltaAvgSegNum_);
                    getValueIntOrDouble(consumerStreamThreadsSettings[videoThreadCount], "average_segnum_delta_parity", videoThread->deltaAvgParitySegNum_);
                    getValueIntOrDouble(consumerStreamThreadsSettings[videoThreadCount], "average_segnum_key", videoThread->keyAvgSegNum_);
                    getValueIntOrDouble(consumerStreamThreadsSettings[videoThreadCount], "average_segnum_key_parity", videoThread->keyAvgParitySegNum_);
                    LogInfo("") << "consumerStreamThreadsSettings[" << videoThreadCount << "](video): " << videoThread->keyAvgParitySegNum_ << std::endl;
                    
                    const Setting &consumerStreamThreadCoderSettings=consumerStreamThreadsSettings[videoThreadCount]["coder"];
                    getValueIntOrDouble(consumerStreamThreadCoderSettings, "frame_rate", videoThread->coderParams_.codecFrameRate_);
                    consumerStreamThreadCoderSettings.lookupValue("gop", videoThread->coderParams_.gop_);
                    consumerStreamThreadCoderSettings.lookupValue("start_bitrate", videoThread->coderParams_.startBitrate_);
                    consumerStreamThreadCoderSettings.lookupValue("max_bitrate", videoThread->coderParams_.maxBitrate_);
                    consumerStreamThreadCoderSettings.lookupValue("encode_height", videoThread->coderParams_.encodeWidth_);
                    consumerStreamThreadCoderSettings.lookupValue("encode_width", videoThread->coderParams_.encodeHeight_);

                    defaultVideoStream.mediaStream_.mediaThreads_.push_back(videoThread);
                }
                params.defaultVideoStreams_.push_back(defaultVideoStream);
            }else{
                LogInfo("") << "Unknow stream type!" << std::endl;
                return(EXIT_FAILURE);
            }
        }
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading stream settings!" << std::endl;
    }
    
////////////////////////////////////
//not used for now
///////////////////////////////////
    // try
    // {// setup producer settings
    //     const Setting &producerAudioSettings = root["producer"]["audio"];
    //     const Setting &producerVideoSettings = root["producer"]["video"];
        
    //     producerAudioSettings.lookupValue("segment_size", params.audioProducerParams_.segmentSize_);
    //     producerAudioSettings.lookupValue("freshness", params.audioProducerParams_.freshnessMs_);
    //     producerVideoSettings.lookupValue("segment_size", params.videoProducerParams_.segmentSize_);
    //     producerVideoSettings.lookupValue("freshness", params.videoProducerParams_.freshnessMs_);
        
    //     const Setting &audioDevices = root["producer"]["acapture"];
    //     for (int i = 0; i < audioDevices.getLength(); i++)
    //     {
    //         AudioCaptureParams* audioDevice = new AudioCaptureParams();
    //         audioDevices[i].lookupValue("device_id", audioDevice->deviceId_);
            
    //         params.audioDevices_.push_back(audioDevice);
    //     }
        
    //     const Setting &videoDevices = root["producer"]["vcapture"];
    //     for (int i = 0; i < videoDevices.getLength(); i++)
    //     {
    //         int framerate = 0;
    //         VideoCaptureParams* videoDevice = new VideoCaptureParams();
    //         videoDevices[i].lookupValue("device_id", videoDevice->deviceId_);
    //         videoDevices[i].lookupValue("capture_width", videoDevice->captureWidth_);
    //         videoDevices[i].lookupValue("capture_height", videoDevice->captureHeight_);
    //         videoDevices[i].lookupValue("framerate", framerate);
    //         videoDevice->framerate_ = (double)framerate;
            
    //         params.videoDevices_.push_back(videoDevice);
    //     }
        
    //     const Setting &audioStreams = root["producer"]["astreams"];
    //     for (int i = 0; i < audioStreams.getLength(); i++)
    //     {
    //         int captureDeviceIdx = 0;
    //         MediaStreamParams* streamParams = new MediaStreamParams();
    //         audioStreams[i].lookupValue("name", streamParams->streamName_);
    //         audioStreams[i].lookupValue("capture_idx", captureDeviceIdx);
            
    //         if (captureDeviceIdx < params.audioDevices_.size())
    //             streamParams->captureDevice_ = params.audioDevices_[captureDeviceIdx];
            
    //         const Setting& threads = audioStreams[i]["threads"];
    //         for (int i = 0; i < threads.getLength(); i++)
    //         {
    //             AudioThreadParams* threadParams = new AudioThreadParams();
    //             threads[i].lookupValue("name", threadParams->threadName_);
                
    //             streamParams->mediaThreads_.push_back(threadParams);
    //         }
            
    //         params.defaultAudioStreams_.push_back(streamParams);
    //     }

    //     const Setting &videoStreams = root["producer"]["vstreams"];
    //     for (int i = 0; i < videoStreams.getLength(); i++)
    //     {
    //         int captureDeviceIdx = 0;
    //         MediaStreamParams* streamParams = new MediaStreamParams();
    //         videoStreams[i].lookupValue("name", streamParams->streamName_);
    //         videoStreams[i].lookupValue("capture_idx", captureDeviceIdx);
            
    //         if (captureDeviceIdx < params.videoDevices_.size())
    //             streamParams->captureDevice_ = params.videoDevices_[captureDeviceIdx];
            
    //         const Setting& threads = videoStreams[i]["threads"];
    //         for (int i = 0; i < threads.getLength(); i++)
    //         {
    //             int framerate = 0;
    //             VideoThreadParams* threadParams = new VideoThreadParams();
    //             threads[i].lookupValue("name", threadParams->threadName_);
    //             threads[i].lookupValue("frame_rate", framerate);
    //             threadParams->codecFrameRate_ = (double)framerate;
    //             threads[i].lookupValue("gop", threadParams->gop_);
    //             threads[i].lookupValue("start_bitrate", threadParams->startBitrate_);
    //             threads[i].lookupValue("max_bitrate", threadParams->maxBitrate_);
    //             threads[i].lookupValue("drop_frames", threadParams->dropFramesOn_);
    //             threadParams->encodeWidth_ = ((VideoCaptureParams*)streamParams->captureDevice_)->captureWidth_;
    //             threadParams->encodeHeight_ = ((VideoCaptureParams*)streamParams->captureDevice_)->captureHeight_;
                
    //             streamParams->mediaThreads_.push_back(threadParams);
    //         }
            
    //         params.defaultVideoStreams_.push_back(streamParams);
    //     }
    // }
    // catch (const SettingNotFoundException &nfex)
    // {
    //     // ignore
    // }

    return EXIT_SUCCESS;
}
