//
//  renderer-stub.h
//  ndnrtc
//
//  Created by Peter Gusev on 6/26/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__renderer_stub__
#define __ndnrtc__renderer_stub__

#include <iostream>
#include "external-renderer.h"

class RendererStub : public ndnrtc::IExternalRenderer
{
public:
    RendererStub(){};
    ~RendererStub(){};
    
    uint8_t* getFrameBuffer(int width, int height);
    void renderRGBFrame(int64_t timestamp, int width, int height,
                        const uint8_t* buffer);
    
private:
    int bufferSize_;
    uint8_t *buffer_;
};

#endif /* defined(__ndnrtc__renderer_stub__) */
