//
// renderer.cpp
//
//  Created by Peter Gusev on 30 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "renderer.hpp"

#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace ndnrtc;

Renderer::Renderer(const std::string& pipeName)
: width_(0)
, height_(0)
, buffer_(nullptr)
, pipePath_(pipeName)
{
    createPipe(pipePath_);
    openPipe(pipePath_);
}

Renderer::~Renderer()
{
    if (pipe_ > 0)
        close(pipe_);

    if (buffer_)
        free(buffer_);
}

void
Renderer::openPipe(const std::string& path)
{
    pipe_ = open(path.c_str(), O_WRONLY | O_NONBLOCK | O_EXCL);
}

void
Renderer::createPipe(const std::string& path)
{
    int res = mkfifo(path.c_str(), 0644);

    if (res < 0 && errno != EEXIST)
    {
        std::stringstream ss;
        ss << "Error creating pipe(" << errno << "): " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
}

uint8_t* 
Renderer::getFrameBuffer(int width, int height, IExternalRenderer::BufferType*)
{
    if (width_ != width || height_ != height)
    {
        bufferSize_ = 4*width*height;
        buffer_ = (uint8_t*)realloc((void*)buffer_, bufferSize_);
        width_ = width;
        height_ = height;
    }

    return buffer_;
}

void 
Renderer::renderFrame(const ndnrtc::FrameInfo&, int width, int height,
                      const uint8_t* buffer)
{
    if (pipe_ < 0)
        openPipe(pipePath_);

    if (pipe_ > 0)
    {
        uint8_t *buf = buffer_;
        int written = 0, r = 0;
        bool continueWrite = false;

        do
        {
            r = write(pipe_, buf + written, bufferSize_ - written);
            if (r > 0)
                written += r;

            // keep writing if has not written all buffer and no errors occured or
            // there was an error and this error is EAGAIN
            continueWrite = (r > 0 && written != bufferSize_) ||
                            (r < 0 && errno == EAGAIN);
        } while (continueWrite);
    }
    else
        std::cerr << "Error writing to pipe (" << pipePath_ << "): " 
                  << strerror(errno) << std::endl;
}
