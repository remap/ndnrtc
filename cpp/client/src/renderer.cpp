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
:sinkName_(sinkFileName), frameCount_(0), sinkIdx_(0), 
isDumping_(true), suppressBadSink_(suppressBadSink),
frame_(new ArgbFrame(0,0))
{
};

RendererInternal::~RendererInternal()
{
    closeSink();
    frame_.reset();
}

uint8_t* RendererInternal::getFrameBuffer(int width, int height)
{
    if (frame_->getFrameSizeInBytes() < ArgbFrame(width, height).getFrameSizeInBytes())
    {
        frame_.reset(new ArgbFrame(width, height));
        closeSink();
        string sinkFullPath = openSink(width, height);
        sinkIdx_++;
        
        LogInfo("") << "receiving frame of resolution " << width << "x" << height 
            << "(" << frame_->getFrameSizeInBytes() <<" bytes per frame)." 
            <<  (isDumping_? string(" writing to ") + sinkFullPath : "" ) << std::endl;
    }

    return frame_->getBuffer().get();
}

void RendererInternal::renderBGRAFrame(int64_t timestamp, int width, int height, 
    const uint8_t* buffer)
{
    if (!frame_.get())
        throw runtime_error("render buffer hasn not been initialized");

    if (width != frame_->getWidth() || height != frame_->getHeight())
        throw runtime_error("wrong frame size supplied");

    // do whatever we need, i.e. drop frame, render it, write to file, etc.
    LogDebug("") << "received frame (" << width << "x" << height << ") at " 
    << timestamp << " ms"<<", frame count: "<< frameCount_ << std::endl;

    dumpFrame();
    frameCount_++;
}
    
string RendererInternal::openSink(unsigned int width, unsigned int height)
{
    if (sinkName_ == "")
    {
        if (suppressBadSink_)
            isDumping_ = false;
        else
            throw runtime_error("invalid sink name provided");
    }

    stringstream sinkPath;
    sinkPath << sinkName_ << "-" << sinkIdx_ << "-"
        << width << "x" << height << ".argb";

    try
    {
        sink_.reset(new FileSink(sinkPath.str()));
    }
    catch (const std::runtime_error& e)
    {
        if (suppressBadSink_)
            isDumping_ = false;
        else
            throw e;
    }

    return sinkPath.str();
}

void RendererInternal::closeSink()
{
    if (isDumping_ && sink_.get())
        sink_.reset();
}

void RendererInternal::dumpFrame()
{
    if (!isDumping_)
        return;
    
    *sink_ << *frame_;
}
