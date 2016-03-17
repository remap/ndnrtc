//
//  renderer.cpp
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//


#include <stdlib.h>
#include <ndnrtc/simple-log.h>

#include "renderer.h"

using namespace std;

RendererInternal::RendererInternal(const std::string& sinkFileName, bool suppressBadSink)
:sinkName_(sinkFileName), renderBuffer_(nullptr), bufferSize_(0), frameCount_(0), sink_(nullptr),
currentWidth_(0), currentHeight_(0), sinkIdx_(0), isDumping_(!suppressBadSink)
{ 
};

RendererInternal::~RendererInternal()
{
    closeSink();
    freeBuffer();
}

uint8_t* RendererInternal::getFrameBuffer(int width, int height)
{
    if (!renderBuffer_ || getBufferSizeForResolution(width, height) > bufferSize_)
    {
        // init ARGB buffer
        currentWidth_ = width;
        currentHeight_ = height;

        closeSink();
        string sinkFullPath = openSink();
        sinkIdx_++;

        initBuffer();
        
        LogInfo("") << "initialized render buffer " << currentWidth_ << "x" << currentHeight_ 
            << "(" << bufferSize_ <<" bytes)." 
            <<  (isDumping_? string(" writing to ") + sinkFullPath : "" ) << std::endl;
    }

    return (uint8_t*)renderBuffer_;
}

void RendererInternal::renderBGRAFrame(int64_t timestamp, int width, int height, 
    const uint8_t* buffer)
{
    if (!renderBuffer_)
        throw runtime_error("render buffer hasn not been initialized");

    if (width != currentWidth_ || height != currentHeight_)
        throw runtime_error("wrong frame size supplied");

    // do whatever we need, i.e. drop frame, render it, write to file, etc.
    LogDebug("") << "received frame (" << width << "x" << height << ") at " 
    << timestamp << " ms"<<", frame count: "<<frameCount_ << std::endl;

    dumpFrame(buffer);

    frameCount_++;
    return;
}
    
string RendererInternal::openSink()
{
    if (!isDumping_)
        return "";

    if (sinkName_ == "")
        throw runtime_error("invalid sink name provided");

    stringstream sinkPath;

    sinkPath << sinkName_ << "-" << sinkIdx_ << "-" << currentWidth_ << "x" << currentHeight_ << ".argb";
    sink_ = fopen(sinkPath.str().c_str(), "wb");

    if (!sink_)
        throw std::runtime_error("couldn't create file at path "+sinkPath.str());

    return sinkPath.str();
}

void RendererInternal::closeSink()
{
    if (isDumping_ && sink_)
    {
        fclose(sink_);
        sink_ = nullptr;
    }
}

void RendererInternal::initBuffer()
{
    if (currentWidth_ > 0 && currentHeight_ > 0)
    {
        bufferSize_ = getBufferSizeForResolution(currentWidth_, currentHeight_);
        renderBuffer_ = (char*)realloc(renderBuffer_, bufferSize_);
    }
    else
    {
        stringstream errmsg;
        errmsg << "incorrect size for the render buffer (" 
            << currentWidth_ << "x" << currentHeight_ << ")";
        throw runtime_error(errmsg.str());
    }
}

void RendererInternal::freeBuffer()
{
    if (renderBuffer_)
        free(renderBuffer_);
}

void RendererInternal::dumpFrame(const uint8_t* frame)
{
    if (!isDumping_)
        return;
    
    fwrite(frame, sizeof(uint8_t), getBufferSizeForResolution(currentWidth_, currentHeight_), sink_);
}

unsigned int RendererInternal::getBufferSizeForResolution(unsigned int width, unsigned int height)
{
    return width*height*4;
}

