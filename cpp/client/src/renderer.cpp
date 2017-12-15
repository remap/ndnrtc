//
//  renderer.cpp
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//


#include <stdlib.h>
#include <ndnrtc/simple-log.hpp>

#include "renderer.hpp"

using namespace std;

RendererInternal::RendererInternal(const std::string sinkName, SinkFactoryCreate sinkFactoryCreate, 
        bool suppressBadSink)
:sinkName_(sinkName), createSink_(sinkFactoryCreate), 
frameCount_(0), isDumping_(true), suppressBadSink_(suppressBadSink),
frame_(new ArgbFrame(0,0))
{
}

RendererInternal::~RendererInternal()
{
    closeSink();
    frame_.reset();
}

uint8_t* RendererInternal::getFrameBuffer(int width, int height)
{
    if (sink_ && sink_->isBusy()) 
    {
        LogWarn("") << "Frame sink is busy. Writing frames is too slow?..." << std::endl;
        return nullptr;
    }

    if (frame_->getFrameSizeInBytes() != ArgbFrame(width, height).getFrameSizeInBytes())
    {
        frame_.reset(new ArgbFrame(width, height));
        closeSink();
        openSink(width, height);
        
        LogInfo("") << "receiving frame of resolution " << width << "x" << height 
            << "(" << frame_->getFrameSizeInBytes() <<" bytes per frame)." 
            <<  (isDumping_? string(" writing to ") + sink_->getName() : "" ) << std::endl;
    }

    return frame_->getBuffer().get();
}

void RendererInternal::renderBGRAFrame(int64_t timestamp, uint frameNo, int width, int height, 
    const uint8_t* buffer)
{
    if (!frame_.get())
        throw runtime_error("render buffer has not been initialized");

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
            throw runtime_error("invalid sink provided");
    }

    stringstream sinkPath;
    sinkPath << sinkName_  << "." << width << "x" << height;

    try
    {
        sink_ = createSink_(sinkPath.str());
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
    
    if (!sink_->isBusy())
        *sink_ << *frame_;
}
