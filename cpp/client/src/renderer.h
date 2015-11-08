// 
// renderer.h
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//
<<<<<<< HEAD
#include <stdlib.h>
=======
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
#include <fstream>

#include <ndnrtc/interfaces.h>

class RendererInternal : public ndnrtc::IExternalRenderer{
public:
    RendererInternal():renderBuffer_(nullptr), bufferSize_(0) { 
<<<<<<< HEAD
        
    	videoFrameFilePointer = new std::ofstream;

    };
    ~RendererInternal(){
    	delete videoFrameFilePointer;
=======
    	// videoFrameFilePointer = new std::ofstream;
    };
    ~RendererInternal(){
    	// delete videoFrameFilePointer;
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
	};
    
    virtual uint8_t* getFrameBuffer(int width, int height);
    virtual void renderBGRAFrame(int64_t timestamp, int width, int height,
                         const uint8_t* buffer);
    
private:
    char *renderBuffer_;
    int bufferSize_;
<<<<<<< HEAD
    int frameCount_=0;
    std::ofstream *videoFrameFilePointer;
    bool firstFrame=true;
=======
    // std::ofstream *videoFrameFilePointer;
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
};