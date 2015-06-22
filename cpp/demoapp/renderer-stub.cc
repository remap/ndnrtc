//
//  renderer-stub.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 6/26/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "renderer-stub.h"

uint8_t*
RendererStub::getFrameBuffer(int width, int height)
{
    int requiredSize = width*height*3;
    
    if (!buffer_ || bufferSize_ < requiredSize)
    {
        buffer_ = (uint8_t*)realloc((void*)buffer_, requiredSize);
        bufferSize_ = requiredSize;
    }
    
    return buffer_;
}

void
RendererStub::renderRGBFrame(int64_t timestamp, int width, int height,
                             const uint8_t *buffer)
{
    // do rendering here
    // ...

    return;
}