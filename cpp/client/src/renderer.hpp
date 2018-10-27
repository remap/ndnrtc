//
//  renderer.h
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __renderer_h__
#define __renderer_h__

#include <stdlib.h>
#include <stdio.h>

#include <ndnrtc/interfaces.hpp>
#include "frame-io.hpp"

/**
 * This class implements IExternalRendere interface
 * It saves raw decoded files into a file.
 * Each frame is an ARGB frame. To watch recorded video, 
 * one should run ffmpeg and transcode raw sequence into one
 * of widely used video containers. For example, the following 
 * command will transcode encoded raw video into mp4 file:
 *
 *     $ ffmpeg -f rawvideo -vcodec rawvideo -s 1280x720 \
 *           -r 15 -pix_fmt argb -i freeculture-video-0-1280x720.argb \
 *           -c:v libx264 -preset ultrafast -qp 0 freeculture.mp4
 *
 * NOTE: raw video files are very heavy and can grow into gigabytes 
 * rapidly over few tens of seconds of recording
 *
 * more info on transcoding raw video can be found at:
 *      http://stackoverflow.com/questions/15778774/using-ffmpeg-to-losslessly-convert-yuv-to-another-format-for-editing-in-adobe-pr
 */

typedef std::function<boost::shared_ptr<IFrameSink>(const std::string&)> SinkFactoryCreate;

class RendererInternal : public ndnrtc::IExternalRenderer{
public:
    /**
     * @param sinkName Base name which will be used to derive new sink names
     * @param sinkFactoryCreate Function that creates new sink
     * @param suppressBadSink If there is a problem creating new sink, renderer will
     *                          throw is this is false or ignore otherwise.  
     */ 
    RendererInternal(const std::string sinkName, SinkFactoryCreate sinkFactoryCreate, 
        boost::asio::io_service& io, bool suppressBadSink = false);
    ~RendererInternal();
    
    virtual uint8_t* getFrameBuffer(int width, int height, IExternalRenderer::BufferType*);
    virtual void renderFrame(const ndnrtc::FrameInfo&, int width, int height,
                         const uint8_t* buffer);
    
private:
    boost::asio::io_service &io_;
    SinkFactoryCreate createSink_;
    boost::shared_ptr<IFrameSink> sink_;
    boost::shared_ptr<ArgbFrame> frame_;
    std::string sinkName_;
    bool isDumping_, suppressBadSink_;
    unsigned int frameCount_;
    
    std::string openSink(unsigned int width, unsigned int height);
    void closeSink();
    void dumpFrame();
};

#endif
