//
//  ndnrtc_common.hpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/7/13
//

#ifndef ndnrtc_ndnrtc_common_h
#define ndnrtc_ndnrtc_common_h

#include <stdlib.h>
#include <stdint.h>

typedef int PacketNumber;
typedef int SegmentNumber;
typedef PacketNumber FrameNumber;

/**
 * Video Codec structures
 */
namespace ndnrtc
{
class IFrameBufferList;
}

// Video Codec settings
 typedef struct _Settings {
     unsigned int numCores_; // number of hardware cores for concurrency
     bool rowMt_;            // row-wise multithreading

     union codec {
         struct _encoder {
             uint16_t width_;    // encoding width
             uint16_t height_;   // encoding height
             uint32_t bitrate_;  // target bitrate
             uint16_t gop_;      // Group of Picture size
             uint16_t fps_;      // target FPS
             bool dropFrames_;   // indicates whether encoder can drop frames
         } encoder_;
         struct _decoder {
             ndnrtc::IFrameBufferList* frameBufferList_;
         } decoder_;
     } spec_;
 } CodecSettings;

// Raw Image formats
enum class ImageFormat {
    I420
};

// Encoded Frame types
enum class FrameType {
    Key,
    Delta
};

// Encoded frame structure
typedef struct _EncodedFrame {
      FrameType type_;
      uint64_t pts_;
      uint32_t duration_;
      size_t length_;
      uint8_t *data_;
      void *userData_;
} EncodedFrame;

#endif
