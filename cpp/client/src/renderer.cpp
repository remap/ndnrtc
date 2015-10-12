// 
// renderer.cpp
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//

#include <stdlib.h>
#include <ndnrtc/simple-log.h>

#include "renderer.h"

uint8_t* RendererInternal::getFrameBuffer(int width, int height)
{
    if (!renderBuffer_ || width*height*4 > bufferSize_)
    {
        // init ARGB buffer
        bufferSize_ = width*height*4;
        LogInfo("") << "initializing render buffer " << width << "x" << height << "(" << bufferSize_ <<" bytes)..." << std::endl;
        renderBuffer_ = (char*)realloc(renderBuffer_, bufferSize_);
    }

    return (uint8_t*)renderBuffer_;
}

void RendererInternal::renderBGRAFrame(int64_t timestamp, int width, int height,
                     const uint8_t* buffer)
{
    // do whatever we need, i.e. drop frame, render it, write to file, etc.
    LogInfo("") << "received frame (" << width << "x" << height << ") at " << timestamp << "ms" << std::endl;
    return;
}
    
