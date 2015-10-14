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

int loadParamsFromFile(const string &cfgFileName,
                       ndnrtc::new_api::AppParams &params)
{
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();
    
    try
    { // setup general settings
        const Setting &general = root["general"];
        string log_level, log_file, headlessUser;
        bool use_tlv, use_rtx, use_fec, use_cache, use_av_sync, use_audio, use_video;
        unsigned int headless_mode;
        
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
        general.lookupValue("prefix", params.generalParams_.prefix_);
        LogInfo("") << "prefix: " << params.generalParams_.prefix_ << std::endl;
        
        const Setting &ndnNetwork = general["ndnnetwork"];
        ndnNetwork.lookupValue("connect_host", params.generalParams_.host_);
        LogInfo("") << "params.generalParams_.host_: " << params.generalParams_.host_ << std::endl;
        ndnNetwork.lookupValue("connect_port", params.generalParams_.portNum_);
        LogInfo("") << "params.generalParams_.portNum_: " << params.generalParams_.portNum_ << std::endl;
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try
    {
        const Setting &headless = root["headless"];
        unsigned int mode;
        
        headless.lookupValue("mode", mode);
        LogInfo("") << "(headless) mode: " << mode << std::endl;

        if (mode == 1)
            params.headlessModeParams_.mode_ = HeadlessModeParams::HeadlessModeConsumer;
        else if (mode == 2)
            params.headlessModeParams_.mode_ = HeadlessModeParams::HeadlessModeProducer;
        
        headless.lookupValue("username", params.headlessModeParams_.username_);
        LogInfo("") << "params.headlessModeParams_.username_: " << params.headlessModeParams_.username_ << std::endl;
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
    }
    
    try
    {// setup consumer settings
        const Setting &consumerAudioSettings = root["consumer"]["audio"];
        const Setting &consumerVideoSettings = root["consumer"]["video"];
        
        consumerAudioSettings.lookupValue("interest_lifetime", params.audioConsumerParams_.interestLifetime_);
        LogInfo("") << "params.audioConsumerParams_.interestLifetime_: " << params.audioConsumerParams_.interestLifetime_ << std::endl;
        consumerAudioSettings.lookupValue("jitter_size", params.audioConsumerParams_.jitterSizeMs_);
        consumerAudioSettings.lookupValue("buffer_size", params.audioConsumerParams_.bufferSlotsNum_);
        consumerAudioSettings.lookupValue("slot_size", params.audioConsumerParams_.slotSize_);
        
        consumerVideoSettings.lookupValue("interest_lifetime", params.videoConsumerParams_.interestLifetime_);
        LogInfo("") << "params.videoConsumerParams_.interestLifetime_: " << params.videoConsumerParams_.interestLifetime_ << std::endl;
        consumerVideoSettings.lookupValue("jitter_size", params.videoConsumerParams_.jitterSizeMs_);
        consumerVideoSettings.lookupValue("buffer_size", params.videoConsumerParams_.bufferSlotsNum_);
        consumerVideoSettings.lookupValue("slot_size", params.videoConsumerParams_.slotSize_);
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
    }

    
    try
    {// setup audio stream settings
        const Setting &audioStreamsSettings = root["allStreams"]["allAudioStreams"];
        int audioCount = audioStreamsSettings.getLength();

        for(int audiocount=0; audiocount<audioCount; audiocount++){
            const Setting &audioStreamSettings = audioStreamsSettings[audiocount];
            std::string prefix;
            audioStreamSettings.lookupValue("asessionPrefix",prefix);
            LogInfo("") << "asessionPrefix-"<<audiocount<< ": " << prefix<< std::endl;
        }
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading audio stream settings!" << std::endl;
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

int loadVideoStreamParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::MediaStreamParams &videoStreamParams,
                       std::string &videoStreamPrefix,
                       std::string &vthreadToFetch,
                       int videoStreamCount){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();

    try
    {// setup video stream settings
        const Setting &videoStreamSettings = root["allStreams"]["allVideoStreams"][videoStreamCount];
        
        videoStreamSettings.lookupValue("vsessionPrefix",videoStreamPrefix);
        LogInfo("") << "videoStreamPrefix: " << videoStreamPrefix<< std::endl;
        videoStreamSettings.lookupValue("vthreadToFetch",vthreadToFetch);
        videoStreamSettings.lookupValue("streamName_",videoStreamParams.streamName_ );
        videoStreamParams.type_ = MediaStreamParams::MediaStreamTypeVideo;

        const Setting &videoStreamProducerSettings = root["allStreams"]["allVideoStreams"][videoStreamCount]["producerParams_"];
        videoStreamProducerSettings.lookupValue("segmentSize_",videoStreamParams.producerParams_.segmentSize_);
        LogInfo("") << "videoStreamParams.producerParams_.segmentSize_: "<< videoStreamParams.producerParams_.segmentSize_ << std::endl;
        videoStreamProducerSettings.lookupValue("freshnessMs_",videoStreamParams.producerParams_.freshnessMs_);


    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading video stream settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int getVideoStreamsNumberFromFile(const std::string &cfgFileName){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();

    try
    {// setup video stream settings
        const Setting &videoStreamsSettings = root["allStreams"]["allVideoStreams"];
        LogInfo("") << "videoStreamsSettings.getLength(): "<< videoStreamsSettings.getLength() << std::endl;
        return videoStreamsSettings.getLength();
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading video stream number settings!" << std::endl;
        return(EXIT_FAILURE);
    }
}

int loadVideoStreamThreadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::VideoThreadParams* vthreadParams,
                       int videoStreamCount,
                       int videoStreamThreadCount){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();
    
    try
    {// setup video stream settings
        const Setting &videoStreamProducerSettings = root["allStreams"]["allVideoStreams"][videoStreamCount]["vthreadParams"][videoStreamThreadCount];

        videoStreamProducerSettings.lookupValue("threadName_",vthreadParams->threadName_);
        LogInfo("") << "vthreadParams->threadName_: "<<vthreadParams->threadName_ << std::endl;
        getValueIntOrDouble(videoStreamProducerSettings, "deltaAvgSegNum_", vthreadParams->deltaAvgSegNum_);
        LogInfo("") << "vthreadParams->deltaAvgSegNum_: "<<vthreadParams->deltaAvgSegNum_ << std::endl;
        getValueIntOrDouble(videoStreamProducerSettings, "deltaAvgParitySegNum_", vthreadParams->deltaAvgParitySegNum_);
        getValueIntOrDouble(videoStreamProducerSettings, "keyAvgSegNum_", vthreadParams->keyAvgSegNum_);
        getValueIntOrDouble(videoStreamProducerSettings, "keyAvgParitySegNum_", vthreadParams->keyAvgParitySegNum_);

        const Setting &videoStreamProducerCoderSettings = root["allStreams"]["allVideoStreams"][videoStreamCount]["vthreadParams"][videoStreamThreadCount]["coderParams_"];

        getValueIntOrDouble(videoStreamProducerCoderSettings, "codecFrameRate_", vthreadParams->coderParams_.codecFrameRate_);
        LogInfo("") << "vthreadParams->coderParams_.codecFrameRate_: " << vthreadParams->coderParams_.codecFrameRate_ << std::endl;
        videoStreamProducerCoderSettings.lookupValue("gop_",vthreadParams->coderParams_.gop_);
        videoStreamProducerCoderSettings.lookupValue("startBitrate_",vthreadParams->coderParams_.startBitrate_);
        videoStreamProducerCoderSettings.lookupValue("maxBitrate_",vthreadParams->coderParams_.maxBitrate_);
        videoStreamProducerCoderSettings.lookupValue("encodeHeight_",vthreadParams->coderParams_.encodeHeight_);
        videoStreamProducerCoderSettings.lookupValue("encodeWidth_",vthreadParams->coderParams_.encodeWidth_);
        LogInfo("") << "vthreadParams->coderParams_.encodeHeight_: "<< vthreadParams->coderParams_.encodeHeight_ << std::endl;
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading video stream thread settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int getVideoStreamThreadNumberFromFile(const std::string &cfgFileName,
                        int videoStreamCount){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();

    try
    {// setup video stream settings
        const Setting &videoStreamThreadSettings = root["allStreams"]["allVideoStreams"][videoStreamCount]["vthreadParams"];
        LogInfo("") << "videoStreamThreadSettings.getLength(): "<< videoStreamThreadSettings.getLength() << std::endl;
        return videoStreamThreadSettings.getLength();
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading video stream number settings!" << std::endl;
        return(EXIT_FAILURE);
    }
}

int loadAudioStreamParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::MediaStreamParams &audioStreamParams,
                       std::string &audioStreamPrefix,
                       std::string &athreadToFetch,
                       int audioStreamCount){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();

    try
    {// setup audio stream settings
        const Setting &audioStreamSettings = root["allStreams"]["allAudioStreams"][audioStreamCount];
        
        audioStreamSettings.lookupValue("asessionPrefix",audioStreamPrefix);
        LogInfo("") << "audioStreamPrefix: " << audioStreamPrefix<< std::endl;
        audioStreamSettings.lookupValue("athreadToFetch",athreadToFetch);
        audioStreamSettings.lookupValue("streamName_",audioStreamParams.streamName_);
        audioStreamParams.type_ = MediaStreamParams::MediaStreamTypeAudio;

        const Setting &audioStreamProducerSettings = root["allStreams"]["allAudioStreams"][audioStreamCount]["producerParams_"];
        audioStreamProducerSettings.lookupValue("segmentSize_",audioStreamParams.producerParams_.segmentSize_);
        LogInfo("") << "audioStreamParams.producerParams_.segmentSize_: "<< audioStreamParams.producerParams_.segmentSize_ << std::endl;
        audioStreamProducerSettings.lookupValue("freshnessMs_",audioStreamParams.producerParams_.freshnessMs_);

    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading video stream settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int getAudioStreamsNumberFromFile(const std::string &cfgFileName){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();

    try
    {// setup audio stream settings
        const Setting &audioStreamsSettings = root["allStreams"]["allAudioStreams"];
        LogInfo("") << "audioStreamsSettings.getLength(): "<< audioStreamsSettings.getLength() << std::endl;
        return audioStreamsSettings.getLength();
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading audio stream number settings!" << std::endl;
        return(EXIT_FAILURE);
    }
}

int loadAudioStreamThreadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::AudioThreadParams* athreadParams,
                       int audioStreamCount,
                       int audioStreamThreadCount){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();
    
    try
    {// setup audio stream settings
        const Setting &audioStreamThreadSettings = root["allStreams"]["allAudioStreams"][audioStreamCount]["athreadParams"][audioStreamThreadCount];
        
        audioStreamThreadSettings.lookupValue("threadName_",athreadParams->threadName_);
        LogInfo("") << "athreadParams->threadName_: "<<athreadParams->threadName_ << std::endl;
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading audio stream thread settings!" << std::endl;
        return(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

int getAudioStreamThreadNumberFromFile(const std::string &cfgFileName,
                        int audioStreamCount){
    Config cfg;
    
    // Read the file. If there is an error, report it and exit.
    try
    {
        cfg.readFile(cfgFileName.c_str());
    }
    catch(const FileIOException &fioex)
    {
        LogError("")
        << "I/O error while reading file." << endl;
        return(EXIT_FAILURE);
    }
    catch(const ParseException &pex)
    {
        LogError("")
        << "Parse error at " << pex.getFile()
        << ":" << pex.getLine()
        << " - " << pex.getError() << endl;
        
        return(EXIT_FAILURE);
    }
    
    const Setting &root = cfg.getRoot();

    try
    {// setup audio stream settings
        const Setting &audioStreamThreadSettings = root["allStreams"]["allAudioStreams"][audioStreamCount]["athreadParams"];
        LogInfo("") << "audioStreamThreadSettings.getLength(): "<< audioStreamThreadSettings.getLength() << std::endl;
        return audioStreamThreadSettings.getLength();
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
        LogInfo("") << "Error when loading video stream number settings!" << std::endl;
        return(EXIT_FAILURE);
    }
}