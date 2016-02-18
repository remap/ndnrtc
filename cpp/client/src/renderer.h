//
//  config.h
//  ndnrtc
//
//  Copyright (c) 2015 Jiachen Wang. All rights reserved.
//

#include <stdlib.h>
#include <fstream>

#include <ndnrtc/interfaces.h>

class RendererInternal : public ndnrtc::IExternalRenderer{
public:
    RendererInternal():renderBuffer_(nullptr), bufferSize_(0) { 
        
    	videoFrameFilePointer = new std::ofstream;

    };
    ~RendererInternal(){
    	delete videoFrameFilePointer;
	};
    
    virtual uint8_t* getFrameBuffer(int width, int height);
    virtual void renderBGRAFrame(int64_t timestamp, int width, int height,
                         const uint8_t* buffer);
    
private:
    char *renderBuffer_;
    int bufferSize_;
    int frameCount_=0;
    std::ofstream *videoFrameFilePointer;
    bool firstFrame=true;
};
