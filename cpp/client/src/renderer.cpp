// 
// renderer.cpp
//
// Copyright (c) 2015. Peter Gusev. All rights reserved
//

#include <stdlib.h>
#include <ndnrtc/simple-log.h>

#include "renderer.h"

using namespace std;

uint8_t* RendererInternal::getFrameBuffer(int width, int height)
{
    if (!renderBuffer_ || width*height*4 > bufferSize_)
    {
        // init ARGB buffer
        bufferSize_ = width*height*4;
        LogInfo("") << "initializing render buffer " << width << "x" << height << "(" << bufferSize_ <<" bytes)..." << std::endl;
        renderBuffer_ = (char*)realloc(renderBuffer_, bufferSize_);
    }

    return (uint8_t*)renderBuffer_;
}

void RendererInternal::renderBGRAFrame(int64_t timestamp, int width, int height,
                     const uint8_t* buffer)
{
    if (firstFrame==true){
        //write file name and info about the video
        std::stringstream videoFrameFileNameStream;

        // videoFrameFileNameStream << width << "--"<< height <<"--ts"<< timestamp << "--ARGB-save.frames";
        videoFrameFileNameStream << width << "--"<< height <<"--ts"<< timestamp << "--ubuntuHeadless--BGRA.frames";

        std::string videoFrameFileName=videoFrameFileNameStream.str();

        videoFrameFilePointer->open(videoFrameFileName.c_str());
        firstFrame=false; // for the rest frames we do not need to write file name and open it
    }
    // do whatever we need, i.e. drop frame, render it, write to file, etc.
<<<<<<< HEAD
    LogDebug("") << "received frame (" << width << "x" << height << ") at " << timestamp << "ms"<<", frameCount_: "<<frameCount_ << std::endl;
    

    //write the incomming raw video frame to file
    // (*videoFrameFilePointer) << "raw ";//http://wiki.multimedia.cx/index.php?title=Raw_RGB This specification might be wrong 
    if ((frameCount_ > 550)&&(frameCount_ < 770)){ // debug purpose
        LogDebug("") << "record frame: " << frameCount_ << " to file..." << std::endl;
        for (int i = 0; i < width*height*4; i=i+4){// *4 is because it has four channels: Alpha, R, G, B
            // decoder outputs ARGB somehow, so swap to BGRA format and save
            // write as BGRA for mplayer
            (*videoFrameFilePointer) << (unsigned char)buffer[i+3] //B +1, +2, +3ok, 0!ok
                                     << (unsigned char)buffer[i+2]//G
                                     << (unsigned char)buffer[i+1]//R
                                     << (unsigned char)buffer[i];//A
            // write as ARGB for publisher 
            // (*videoFrameFilePointer) << (unsigned char)buffer[i]//A
            //                          << (unsigned char)buffer[i+1]//R
            //                          << (unsigned char)buffer[i+2]//G
            //                          << (unsigned char)buffer[i+3];//B +1, +2, +3ok, 0!ok

        }
    }
    frameCount_++;
=======
    LogInfo("") << "received frame (" << width << "x" << height << ") at " << timestamp << "ms" << std::endl;
    
    std::ofstream *videoFrameFilePointer= new std::ofstream;
    stringstream videoFrameFileNameStream;

    videoFrameFileNameStream << timestamp << "--" << width<<"--" << height <<".frame";

    string videoFrameFileName=videoFrameFileNameStream.str();

    videoFrameFilePointer->open(videoFrameFileName.c_str());
    for (int i = 0; i < width*height; ++i){
        (*videoFrameFilePointer) << buffer[i]; 
        (*videoFrameFilePointer) << buffer[i+1];
        (*videoFrameFilePointer) << buffer[i+2];
        (*videoFrameFilePointer) << buffer[i+3];
        (*videoFrameFilePointer) << "done";
    }
    
    delete videoFrameFilePointer;
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
    return;
}
    
