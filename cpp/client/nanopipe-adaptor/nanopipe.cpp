// 
// nanopipe.c
//
// Copyright (c) 2017. Peter Gusev. All rights reserved
//

// this a little adapter for ndnrtc-client nanomsg frame sink

// build as (requires nanomsg lib installed at /usr/local/lib):
//  $ g++ nanopipe.c ipc-shim.c -std=c++11 -I/usr/local/include -L/usr/local/lib -lnanomsg -onanopipe

// use as:
//  ./nanopipe <nanomsg-socket-name> <pipe-name>

// ffmpeg command to read from pipe and re-encode into mp4 file (mind frame resolution and pipe name):
// $ ffmpeg -f rawvideo -vcodec rawvideo -s 320x240 -pix_fmt argb -i /tmp/ndnrtc-frames -vf vflip -vf hflip -c:v libx264 -preset ultrafast -qp 0 video.mp4

// ffmpeg command to read from pipe and transmit it as h264 stream (mind frame resolution and pipe name):
// $ ffmpeg -f rawvideo -vcodec rawvideo -s 320x240 -pix_fmt argb -i /tmp/ndnrtc-frames -vf vflip -vf hflip -c:v libx264 -preset ultrafast -qp 0 -f mpegts udp://<target-ip>:<port>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <stdint.h>
#include <execinfo.h>
#include <signal.h>

#include <ndnrtc/interfaces.hpp>
#include "ipc-shim.h"

using namespace std;
static bool processFrames = true;

void createPipe(const std::string& path)
{
    int res = mkfifo(path.c_str(), 0644);

    if (res < 0 && errno != EEXIST)
    {
        std::stringstream ss;
        ss << "Error creating pipe(" << errno << "): " << strerror(errno);
        throw std::runtime_error(ss.str());
    }
}

int openPipe(const std::string& path)
{
    return open(path.c_str(), O_WRONLY|O_NONBLOCK|O_EXCL);
}

int writeExactly(uint8_t *buffer, size_t bufSize, int pipe)
{   
    int written = 0, r = 0; 
    bool keepWriting = false;

    do {
        r = write(pipe, buffer+written, bufSize-written);
        if (r > 0) written += r;
        keepWriting = (r > 0 && written != bufSize) || (r < 0 && errno == EAGAIN);
    } while (keepWriting);

    if (written != bufSize)
    {
        std::stringstream ss;
        ss << "something bad happened when writing to pipe: " 
           << strerror(errno) << "(" << errno << ")" << std::endl;
        throw std::runtime_error(ss.str());
    }

    return written;
}

// assuming sockets are named like:
//      <arbitrary-name>.<width>x<height>
void getFrameResolutionFromSocketName(string socketName, int& width, int&height)
{
    string::size_type pos;

    if ((pos = socketName.find(".", 0)) == string::npos)
        throw std::runtime_error("can't derive frame dimensions from socket file name");

    string dim = socketName.substr(pos+1, socketName.size()-pos);
    if ((pos = dim.find("x", 0)) == string::npos)
        throw std::runtime_error("can't derive frame dimensions from socket file name");

    string w = dim.substr(0, pos);
    string h = dim.substr(pos+1, dim.size()-pos);

    width = stoi(w);
    height = stoi(h);
}

ndnrtc::FrameInfo unpackFrameInfo(unsigned char* buf, size_t maxLen)
{
    ndnrtc::FrameInfo finfo;

    finfo.timestamp_ = ((ndnrtc::FrameInfo*)buf)->timestamp_;
    finfo.playbackNo_ = ((ndnrtc::FrameInfo*)buf)->playbackNo_;
    size_t stringOffset = sizeof(finfo) - sizeof(std::string);
    finfo.ndnName_ = std::string((const char*)(buf+stringOffset));

    return finfo;
}

void listenSocket(string socketName, function<void(int, uint8_t*, size_t)> onNewFrame)
{
    int socket = ipc_setupSubSinkSocket(socketName.c_str());

    if (socket < 0)
    {
        stringstream ss;
        ss << "couldn't setup nanomsg socket " 
            << ipc_lastError() << " (" << ipc_lastErrorCode() << ")";
        throw std::runtime_error(ss.str());
    }

    int frameWidth, frameHeight;
    getFrameResolutionFromSocketName(socketName, frameWidth, frameHeight);

    size_t bufferSize = sizeof(uint8_t)*frameWidth*frameHeight*4;
    static uint8_t *buffer = (uint8_t*)malloc(bufferSize);

    cout << "listening for frames " << frameWidth << "x" << frameHeight 
            << " on " << socketName << endl;

    while (processFrames)
    {
        static unsigned char frameHeader[512];
        int ret = ipc_readFrame(socket, frameHeader, buffer, bufferSize);
        ndnrtc::FrameInfo finfo = unpackFrameInfo(frameHeader, 512);

        if (ret < 0) 
            cout << "error reading from socket: " 
                << ipc_lastError() << " (" << ipc_lastErrorCode() << ")" << endl;
        else
        {
            cout << "read frame " << finfo.playbackNo_ << " (" << ret << " bytes),"
            " timestamp " << finfo.timestamp_
            << " ndn name " << finfo.ndnName_ << endl;
            onNewFrame(finfo.playbackNo_, buffer, bufferSize);
        }
    }
}

//******************************************************************************
void handler(int sig) {
    processFrames = false;
    
    if (sig != SIGUSR1 && sig != SIGINT)
    {
        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        fprintf(stderr, "Error: signal %d:\n", sig);
        backtrace_symbols_fd(array, size, STDERR_FILENO);
    } 
    else
        cout << "interrupted" << endl;

    exit(1);
}

void usage(char const *argv[]){
    cout << "\tusage: " << argv[0] << " <nanomsg socket name> <pipe name>" << endl;
}

int main(int argc, char const *argv[])
{
    signal(SIGUSR1, handler);
    signal(SIGINT, handler);
    signal(SIGABRT, handler);
    signal(SIGSEGV, handler);

    if (argc < 3)
    {
        usage(argv);
        exit(2);
    }

    string nanomsgSocket(argv[1]);
    string pipeName(argv[2]);

    try {
        int pipe = -1;
        createPipe(pipeName);
        listenSocket(nanomsgSocket, [pipeName, &pipe](int frameNo, uint8_t *frameBuffer, size_t bufferSize){
            if (pipe < 0) pipe = openPipe(pipeName);
            if (pipe > 0) writeExactly(frameBuffer, bufferSize, pipe);
        });
    }
    catch (std::exception &e){
        cout << "Failed due to exception: " << e.what() << endl;
    }

    return 0;
}
