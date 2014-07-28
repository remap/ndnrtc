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
        string log_level, log_file;
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
            CodecParams streamParams;
            memset(&streamParams, 0, sizeof(CodecParams));
            
            streamParams.idx = i;
            streams[i].lookupValue("frame_rate", streamParams.codecFrameRate);
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
