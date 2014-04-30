//
//  params.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <string.h>
#include "params.h"

using namespace ndnrtc;
using namespace ndnlog;

int _ParamsStruct::validateVideoParams(const struct _ParamsStruct &params,
                                       struct _ParamsStruct &validatedResult)
{
    // check critical values
    if (params.loggingLevel != NdnLoggerDetailLevelNone &&
        (params.logFile == NULL ||
         strcmp(params.logFile,"") == 0))
        return RESULT_ERR;
    
    if (params.host == NULL ||
        strcmp(params.host, "") == 0)
        return RESULT_ERR;
    
    if (params.producerId == NULL ||
        strcmp(params.producerId, "") == 0)
        return RESULT_ERR;
    
    if (params.streamName == NULL ||
        strcmp(params.streamName, "") == 0)
        return RESULT_ERR;
    
    if (params.streamThread == NULL ||
        strcmp(params.streamThread, "") == 0)
        return RESULT_ERR;
    
    if (params.ndnHub == NULL ||
        strcmp(params.ndnHub, "") == 0)
        return RESULT_ERR;
    
    int res = RESULT_OK;
    ParamsStruct validated = DefaultParams;
    
    // check other values
    validated.loggingLevel = params.loggingLevel;
    validated.logFile = params.logFile;
    validated.useTlv = params.useTlv;
    validated.useRtx = params.useRtx;
    validated.useFec = params.useFec;
    validated.useCache = params.useCache;
    validated.headlessMode = params.headlessMode;
    
    validated.captureDeviceId = params.captureDeviceId;
    validated.captureWidth = ParamsStruct::validate(params.captureWidth,
                                                    MinWidth, MaxWidth, res,
                                                    DefaultParams.captureWidth);
    validated.captureHeight = ParamsStruct::validate(params.captureHeight,
                                                     MinHeight, MaxHeight, res,
                                                     DefaultParams.captureHeight);
    validated.captureFramerate = ParamsStruct::validate(params.captureFramerate,
                                                        MinFrameRate, MaxFrameRate, res,
                                                        DefaultParams.captureFramerate);
    
    validated.renderWidth = ParamsStruct::validate(params.renderWidth,
                                                   MinWidth, MaxWidth, res,
                                                   DefaultParams.renderWidth);
    validated.renderHeight = ParamsStruct::validate(params.renderHeight,
                                                    MinHeight, MaxHeight, res,
                                                    DefaultParams.renderHeight);
    
    validated.codecFrameRate = ParamsStruct::validate(params.codecFrameRate,
                                                      MinFrameRate, MaxFrameRate, res,
                                                      DefaultParams.codecFrameRate);
    validated.startBitrate = ParamsStruct::validate(params.startBitrate,
                                                    MinStartBitrate, MaxStartBitrate, res,
                                                    DefaultParams.startBitrate);
    validated.maxBitrate = ParamsStruct::validate(params.maxBitrate,
                                                  MinStartBitrate, MaxBitrate, res,
                                                  DefaultParams.maxBitrate);
    validated.encodeWidth = ParamsStruct::validate(params.encodeWidth,
                                                   MinWidth, MaxWidth, res,
                                                   DefaultParams.encodeWidth);
    validated.encodeHeight = ParamsStruct::validate(params.encodeHeight,
                                                    MinHeight, MaxHeight, res,
                                                    DefaultParams.encodeHeight);
    validated.dropFramesOn = params.dropFramesOn;
    
    validated.host = params.host;
    validated.portNum = ParamsStruct::validateLE(params.portNum, MaxPortNum, res,
                                                 DefaultParams.portNum);
    
    validated.producerId = params.producerId;
    validated.streamName = params.streamName;
    validated.streamThread = params.streamThread;
    validated.ndnHub = params.ndnHub;
    
    validated.segmentSize = ParamsStruct::validate(params.segmentSize,
                                                   MinSegmentSize, MaxSegmentSize, res,
                                                   DefaultParams.segmentSize);
    validated.freshness = params.freshness;
    validated.producerRate = ParamsStruct::validate(params.producerRate,
                                                    MinFrameRate, MaxFrameRate, res,
                                                    DefaultParams.producerRate);
    validated.playbackRate = ParamsStruct::validate(params.playbackRate,
                                                    MinFrameRate, MaxFrameRate, res,
                                                    DefaultParams.playbackRate);
    validated.interestTimeout = params.interestTimeout;
    validated.bufferSize = ParamsStruct::validate(params.bufferSize,
                                                  MinBufferSize, MaxBufferSize, res,
                                                  DefaultParams.bufferSize);
    validated.jitterSize = ParamsStruct::validate(params.jitterSize,
                                                  MinJitterSize, INT_MAX, res,
                                                  DefaultParams.jitterSize);
    validated.slotSize = ParamsStruct::validate(params.slotSize,
                                                MinSlotSize, MaxSlotSize, res,
                                                DefaultParams.slotSize);
    validated.skipIncomplete = params.skipIncomplete;
    
    if (validated.slotSize < validated.segmentSize)
    {
        validated.slotSize = validated.segmentSize;
        res = RESULT_WARN;
    }
    
    validatedResult = validated;
    
    return res;
}

int _ParamsStruct::validateAudioParams(const struct _ParamsStruct &params,
                                       struct _ParamsStruct &validatedResult)
{
    // check critical values
    if (params.host == NULL ||
        strcmp(params.host, "") == 0)
        return RESULT_ERR;
    
    if (params.producerId == NULL ||
        strcmp(params.producerId, "") == 0)
        return RESULT_ERR;
    
    if (params.streamName == NULL ||
        strcmp(params.streamName, "") == 0)
        return RESULT_ERR;
    
    if (params.streamThread == NULL ||
        strcmp(params.streamThread, "") == 0)
        return RESULT_ERR;
    
    if (params.ndnHub == NULL ||
        strcmp(params.ndnHub, "") == 0)
        return RESULT_ERR;
    
    // check other values
    int res = RESULT_OK;
    ParamsStruct validated = DefaultParamsAudio;
    
    validated.loggingLevel = params.loggingLevel;
    validated.logFile = params.logFile;
    validated.useTlv = params.useTlv;
    validated.useRtx = params.useRtx;
    validated.useFec = params.useFec;
    validated.useCache = params.useCache;
    validated.headlessMode = params.headlessMode;
    
    validated.host = params.host;
    validated.portNum = ParamsStruct::validateLE(params.portNum, MaxPortNum,
                                                 res, DefaultParamsAudio.portNum);
    
    validated.producerId = params.producerId;
    validated.streamName = params.streamName;
    validated.streamThread = params.streamThread;
    validated.ndnHub = params.ndnHub;
    
    validated.segmentSize = ParamsStruct::validate(params.segmentSize,
                                                   MinSegmentSize, MaxSegmentSize, res,
                                                   DefaultParamsAudio.segmentSize);
    validated.freshness = params.freshness;
    validated.producerRate = ParamsStruct::validate(params.producerRate,
                                                    MinFrameRate, MaxFrameRate, res,
                                                    DefaultParamsAudio.producerRate);
    validated.playbackRate = ParamsStruct::validate(params.playbackRate,
                                                    MinFrameRate, MaxFrameRate, res,
                                                    DefaultParamsAudio.playbackRate);
    validated.interestTimeout = params.interestTimeout;
    validated.bufferSize = ParamsStruct::validate(params.bufferSize,
                                                  MinBufferSize, MaxBufferSize, res,
                                                  DefaultParamsAudio.bufferSize);
    validated.jitterSize = ParamsStruct::validate(params.jitterSize,
                                                  MinJitterSize, INT_MAX, res,
                                                  DefaultParams.jitterSize);

    validated.slotSize = ParamsStruct::validate(params.slotSize, MinSlotSize,
                                                MaxSlotSize, res,
                                                DefaultParamsAudio.slotSize);
    validated.skipIncomplete = params.skipIncomplete;
    
    if (validated.slotSize < validated.segmentSize)
    {
        validated.slotSize = validated.segmentSize;
        res = RESULT_WARN;
    }
    
    validatedResult = validated;
    
    return res;
}
