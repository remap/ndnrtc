//
// renderer.hpp
//
//  Created by Peter Gusev on 30 October 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#ifndef __renderer_hpp__
#define __renderer_hpp__

#include "../include/interfaces.hpp"

class Renderer : public ndnrtc::IExternalRenderer {
public:
    Renderer(const std::string& pipeName);
    ~Renderer();

private:
    Renderer(const Renderer&) = delete;
    int pipe_, width_, height_;
    size_t bufferSize_;
    uint8_t *buffer_;
    std::string pipePath_;

    void createPipe(const std::string&);
    void openPipe(const std::string&);

    uint8_t* getFrameBuffer(int width, int height, IExternalRenderer::BufferType*) override;
    void renderFrame(const ndnrtc::FrameInfo&, int width, int height,
                     const uint8_t* buffer) override;
};

#endif