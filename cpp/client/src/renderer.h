// 
// renderer.h
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//
#include <fstream>

#include <ndnrtc/interfaces.h>

class RendererInternal : public ndnrtc::IExternalRenderer{
public:
    RendererInternal():renderBuffer_(nullptr), bufferSize_(0) { 
    	// videoFrameFilePointer = new std::ofstream;
    };
    ~RendererInternal(){
    	// delete videoFrameFilePointer;
	};
    
    virtual uint8_t* getFrameBuffer(int width, int height);
    virtual void renderBGRAFrame(int64_t timestamp, int width, int height,
                         const uint8_t* buffer);
    
private:
    char *renderBuffer_;
    int bufferSize_;
    // std::ofstream *videoFrameFilePointer;
};