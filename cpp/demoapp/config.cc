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

int loadParamsFromFile(const string &cfgFileName, ParamsStruct &params,
                       ParamsStruct &audioParams)
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
    
    try{ // setup general settings
        const Setting &general = root["general"];
        string log_level, log_file, headlessUser;
        bool use_tlv, use_rtx, use_fec, use_cache, use_av_sync, use_audio, use_video;
        unsigned int headless_mode;
        
        if (general.lookupValue("log_level", log_level))
        {
            if (log_level == "default")
                params.loggingLevel = NdnLoggerDetailLevelDefault;
            if (log_level == "debug")
                params.loggingLevel = NdnLoggerDetailLevelDebug;
            if (log_level == "all")
                params.loggingLevel = NdnLoggerDetailLevelAll;
            if (log_level == "none")
                params.loggingLevel = NdnLoggerDetailLevelNone;
        }
        
        if (general.lookupValue("log_file", log_file))
        {
            params.logFile = (char*)malloc(log_file.size()+1);
            memset((void*)params.logFile, 0, log_file.size());
            log_file.copy((char*)params.logFile, log_file.size());
        }
        
        if (general.lookupValue("use_tlv", use_tlv))
        {
            params.useTlv = use_tlv;
        }
        
        if (general.lookupValue("use_rtx", use_rtx))
        {
            params.useRtx = use_rtx;
        }
        
        if (general.lookupValue("use_fec", use_fec))
        {
            params.useFec = use_fec;
        }
        
        if (general.lookupValue("headless_mode", headless_mode))
        {
            params.headlessMode = headless_mode;
        }

        if (general.lookupValue("headless_user", headlessUser))
        {
            params.producerId = (char*)malloc(headlessUser.length()+1);
            memcpy((void*)params.producerId, (void*)headlessUser.c_str(), headlessUser.length()+1);
        }
        
        if (general.lookupValue("use_cache", use_cache))
        {
            params.useCache = use_cache;
        }
        
        if (general.lookupValue("use_avsync", use_av_sync))
        {
            params.useAvSync = use_av_sync;
        }
        
        if (general.lookupValue("audio", use_audio))
        {
            params.useAudio = use_audio;
        }
        
        if (general.lookupValue("video", use_video))
        {
            params.useVideo = use_video;
        }
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // capture settings
        const Setting &capture = root["capture"];
        
        capture.lookupValue("device_id", params.captureDeviceId);
        capture.lookupValue("capture_width", params.captureWidth);
        capture.lookupValue("capture_height", params.captureHeight);
        capture.lookupValue("fps", params.captureFramerate);
//        capture.lookupValue("gop", params.gop);
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // renderer settings
        const Setting &renderer = root["renderer"];
        
        renderer.lookupValue("render_width", params.renderWidth);
        renderer.lookupValue("render_height", params.renderHeight);
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
        params.renderWidth = params.captureWidth;
        params.renderHeight = params.captureHeight;
    }
    
    try{ // streams settings
        params.nStreams = 0;
        params.streamsParams = 0;
        
        const Setting &streams = root["vstreams"];
        
        for (int i = 0; i < streams.getLength(); i++)
        {
            int rate = 0;
            CodecParams streamParams;
            memset(&streamParams, 0, sizeof(CodecParams));
            
            streamParams.idx = i;
            streams[i].lookupValue("frame_rate", rate);
            streamParams.codecFrameRate = (double)rate;
            streams[i].lookupValue("gop", streamParams.gop);
            streams[i].lookupValue("start_bitrate", streamParams.startBitrate);
            streams[i].lookupValue("max_bitrate", streamParams.maxBitrate);
            streams[i].lookupValue("drop_frames", streamParams.dropFramesOn);
            
            if (!streams[i].lookupValue("encode_width", streamParams.encodeWidth))
                streamParams.encodeWidth = params.captureWidth;
            
            if (!streams[i].lookupValue("encode_height", streamParams.encodeHeight))
                streamParams.encodeHeight = params.captureHeight;
            
            params.addNewStream(streamParams);
        }
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // network settings
        const Setting &network = root["ndnnetwork"];
        string host;
        string prefix;
        
        if (network.lookupValue("connect_host", host))
        {
            params.host = (char*)malloc(host.size()+1);
            memset((void*)params.host, 0, host.size()+1);
            host.copy((char*)params.host, host.size());
        }
        network.lookupValue("connect_port", params.portNum);
        
        if (network.lookupValue("ndn_prefix", prefix))
        {
            params.ndnHub = (char*)malloc(prefix.size()+1);
            memset((void*)params.ndnHub, 0, prefix.size()+1);
            prefix.copy((char*)params.ndnHub, prefix.size());
        }
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // producer settings
        const Setting &producer = root["videopublish"];
        
        producer.lookupValue("segment_size", params.segmentSize);
        producer.lookupValue("freshness", params.freshness);
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // consumer settings
        const Setting &consumer = root["videofetch"];
        
        consumer.lookupValue("playback_rate", params.producerRate);
        consumer.lookupValue("interest_timeout", params.interestTimeout);
        consumer.lookupValue("jitter_size", params.jitterSize);
        consumer.lookupValue("buffer_size", params.bufferSize);
        consumer.lookupValue("slot_size", params.slotSize);
        consumer.lookupValue("skip_incomplete", params.skipIncomplete);
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    audioParams = params; // make identical for values, not configurable by file
    audioParams.streamName = DefaultParamsAudio.streamName;
//    audioParams.streamThread = DefaultParamsAudio.streamThread;
    audioParams.nStreams = 0;
    audioParams.streamsParams = 0;
    
    // setup audio parameters
    try{ // producer settings
        const Setting &producer = root["audiopublish"];
        
        producer.lookupValue("segment_size", audioParams.segmentSize);
        producer.lookupValue("freshness", audioParams.freshness);
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // consumer settings
        const Setting &consumer = root["audiofetch"];
        
        consumer.lookupValue("interest_timeout", audioParams.interestTimeout);
        consumer.lookupValue("jitter_size", audioParams.jitterSize);
        consumer.lookupValue("buffer_size", audioParams.bufferSize);
        consumer.lookupValue("slot_size", audioParams.slotSize);
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
    try{ // streams settings
        const Setting &streams = root["astreams"];
        
        for (int i = 0; i < streams.getLength(); i++)
        {
            CodecParams streamParams;
            memset(&streamParams, 0, sizeof(CodecParams));
            streamParams.idx = i;
            streams[i].lookupValue("start_bitrate", streamParams.startBitrate);
            
            audioParams.addNewStream(streamParams);
        }
    }
    catch(const SettingNotFoundException &nfex)
    {
        // ignore.
    }
    
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
        
        const Setting &ndnNetwork = general["ndnnetwork"];
        ndnNetwork.lookupValue("connect_host", params.generalParams_.host_);
        ndnNetwork.lookupValue("connect_port", params.generalParams_.portNum_);
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

        if (mode == 1)
            params.headlessModeParams_.mode_ = HeadlessModeParams::HeadlessModeConsumer;
        else if (mode == 2)
            params.headlessModeParams_.mode_ = HeadlessModeParams::HeadlessModeProducer;
        
        headless.lookupValue("username", params.headlessModeParams_.username_);
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
    }
    
    try
    {
        const Setting &consumerAudioSettings = root["consumer"]["audio"];
        const Setting &consumerVideoSettings = root["consumer"]["video"];
        
        consumerAudioSettings.lookupValue("interest_lifetime", params.audioConsumerParams_.interestLifetime_);
        consumerAudioSettings.lookupValue("jitter_size", params.audioConsumerParams_.jitterSizeMs_);
        consumerAudioSettings.lookupValue("buffer_size", params.audioConsumerParams_.bufferSlotsNum_);
        consumerAudioSettings.lookupValue("slot_size", params.audioConsumerParams_.slotSize_);
        
        consumerVideoSettings.lookupValue("interest_lifetime", params.videoConsumerParams_.interestLifetime_);
        consumerVideoSettings.lookupValue("jitter_size", params.videoConsumerParams_.jitterSizeMs_);
        consumerVideoSettings.lookupValue("buffer_size", params.videoConsumerParams_.bufferSlotsNum_);
        consumerVideoSettings.lookupValue("slot_size", params.videoConsumerParams_.slotSize_);
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
    }

    try
    {
        const Setting &producerAudioSettings = root["producer"]["audio"];
        const Setting &producerVideoSettings = root["producer"]["video"];
        
        producerAudioSettings.lookupValue("segment_size", params.audioProducerParams_.segmentSize_);
        producerAudioSettings.lookupValue("freshness", params.audioProducerParams_.freshnessMs_);
        producerVideoSettings.lookupValue("segment_size", params.videoProducerParams_.segmentSize_);
        producerVideoSettings.lookupValue("freshness", params.videoProducerParams_.freshnessMs_);
        
        const Setting &audioDevices = root["producer"]["acapture"];
        for (int i = 0; i < audioDevices.getLength(); i++)
        {
            AudioCaptureParams* audioDevice = new AudioCaptureParams();
            audioDevices[i].lookupValue("device_id", audioDevice->deviceId_);
            
            params.audioDevices_.push_back(audioDevice);
        }
        
        const Setting &videoDevices = root["producer"]["vcapture"];
        for (int i = 0; i < videoDevices.getLength(); i++)
        {
            int framerate = 0;
            VideoCaptureParams* videoDevice = new VideoCaptureParams();
            videoDevices[i].lookupValue("device_id", videoDevice->deviceId_);
            videoDevices[i].lookupValue("capture_width", videoDevice->captureWidth_);
            videoDevices[i].lookupValue("capture_height", videoDevice->captureHeight_);
            videoDevices[i].lookupValue("framerate", framerate);
            videoDevice->framerate_ = (double)framerate;
            
            params.videoDevices_.push_back(videoDevice);
        }
        
        const Setting &audioStreams = root["producer"]["astreams"];
        for (int i = 0; i < audioStreams.getLength(); i++)
        {
            int captureDeviceIdx = 0;
            MediaStreamParams* streamParams = new MediaStreamParams();
            audioStreams[i].lookupValue("name", streamParams->streamName_);
            audioStreams[i].lookupValue("capture_idx", captureDeviceIdx);
            
            if (captureDeviceIdx < params.audioDevices_.size())
                streamParams->captureDevice_ = params.audioDevices_[captureDeviceIdx];
            
            const Setting& threads = audioStreams[i]["threads"];
            for (int i = 0; i < threads.getLength(); i++)
            {
                AudioThreadParams* threadParams = new AudioThreadParams();
                threads[i].lookupValue("name", threadParams->threadName_);
                
                streamParams->mediaThreads_.push_back(threadParams);
            }
            
            params.defaultAudioStreams_.push_back(streamParams);
        }

        const Setting &videoStreams = root["producer"]["vstreams"];
        for (int i = 0; i < videoStreams.getLength(); i++)
        {
            int captureDeviceIdx = 0;
            MediaStreamParams* streamParams = new MediaStreamParams();
            videoStreams[i].lookupValue("name", streamParams->streamName_);
            videoStreams[i].lookupValue("capture_idx", captureDeviceIdx);
            
            if (captureDeviceIdx < params.videoDevices_.size())
                streamParams->captureDevice_ = params.videoDevices_[captureDeviceIdx];
            
            const Setting& threads = videoStreams[i]["threads"];
            for (int i = 0; i < threads.getLength(); i++)
            {
                int framerate = 0;
                VideoThreadParams* threadParams = new VideoThreadParams();
                threads[i].lookupValue("name", threadParams->threadName_);
                threads[i].lookupValue("frame_rate", framerate);
                threadParams->codecFrameRate_ = (double)framerate;
                threads[i].lookupValue("gop", threadParams->gop_);
                threads[i].lookupValue("start_bitrate", threadParams->startBitrate_);
                threads[i].lookupValue("max_bitrate", threadParams->maxBitrate_);
                threads[i].lookupValue("drop_frames", threadParams->dropFramesOn_);
                threadParams->encodeWidth_ = ((VideoCaptureParams*)streamParams->captureDevice_)->captureWidth_;
                threadParams->encodeHeight_ = ((VideoCaptureParams*)streamParams->captureDevice_)->captureHeight_;
                
                streamParams->mediaThreads_.push_back(threadParams);
            }
            
            params.defaultVideoStreams_.push_back(streamParams);
        }
    }
    catch (const SettingNotFoundException &nfex)
    {
        // ignore
    }

    return EXIT_SUCCESS;
}

