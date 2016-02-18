//
//  capturer.cpp
//  ndnrtc
//
//  Copyright (c) 2015 Jiachen Wang. All rights reserved.
//

#include <fstream>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "capturer.h"

using namespace std;

readerVideoFromFile::readerVideoFromFile(boost::asio::io_service &io,
        ndnrtc::IExternalCapturer **localCapturer,
        unsigned int headlessAppOnlineTimeSec)
    : timer_(io, boost::posix_time::milliseconds(interval_)),
      localCapturer_(localCapturer),
      headlessAppOnlineTimeMillisec_(headlessAppOnlineTimeSec * 1000) { // *1000: convert second to millisecond

    ios::streampos fileBegin, fileEnd, fileSize;

    videoFrameFilePointer_ = new std::ifstream;
    // videoFrameFilePointer_->open("720--405--ts89459739--BGRA.frames");
    videoFrameFilePointer_->open("720--405--ts61588975--ARGB-save.frames");// ARGB or BGRA source frame: consumer will receive black frames
    videoFrameFilePointer_->seekg (0, ios::beg);
    fileBegin = videoFrameFilePointer_->tellg();
    videoFrameFilePointer_->seekg (0, ios::end);
    fileEnd = videoFrameFilePointer_->tellg();
    fileSize=fileEnd-fileBegin;
    videoFramesBuffer_=new char[fileSize];

    videoFrameBuffer_=new unsigned char[videoWidth_*videoHeight_*videoChannel_];
    memcpy(videoFrameBuffer_,videoFramesBuffer_,videoWidth_*videoHeight_*videoChannel_);
    videoFrameFilePointer_->read(videoFramesBuffer_,fileSize);
    (*localCapturer_)->capturingStarted();

    frameCount_++;
    timer_.async_wait(boost::bind(&readerVideoFromFile::feedVideoFrame, this));
}

readerVideoFromFile::~readerVideoFromFile() {
    delete[] videoFramesBuffer_;
    delete videoFrameFilePointer_;
}

void readerVideoFromFile::feedVideoFrame() {
    if(headlessAppOnlineTimeMillisec_ > 0) {
        LogDebug("") << "Publish: feeding frames: " << frameCount_ << ", headlessAppOnlineTimeMillisec_: " << headlessAppOnlineTimeMillisec_ << "..." << std::endl;
        memcpy(videoFrameBuffer_,
        		videoFramesBuffer_+frameCount_*videoWidth_*videoHeight_*videoChannel_,
        		videoWidth_*videoHeight_*videoChannel_); // copy every frame
        (*localCapturer_)->incomingArgbFrame(videoWidth_,
                                          	 videoHeight_,
                                          	 videoFrameBuffer_,
                                          	 videoWidth_*videoHeight_*videoChannel_);
        timer_.expires_at(timer_.expires_at() + boost::posix_time::milliseconds(interval_));
        timer_.async_wait(boost::bind(&readerVideoFromFile::feedVideoFrame, this));
        frameCount_++;
        headlessAppOnlineTimeMillisec_ = headlessAppOnlineTimeMillisec_ - interval_;
    }
    if (frameCount_>108){
        frameCount_=0;
        LogDebug("") << "Publish: restore frameCount_ to start play over, frameCount_: " << frameCount_ <<  "..." << std::endl;

    }
}


void callReaderVideoFromFile(ndnrtc::IExternalCapturer **localCapturer, unsigned int headlessAppOnlineTimeSec) {
    boost::asio::io_service io;
    readerVideoFromFile videoReader(io, localCapturer, headlessAppOnlineTimeSec);
    io.run();
    return;
}