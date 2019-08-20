//
//  webrtc.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/14/13
//

// disabling this file
// #define ndnrtc_webrtc_h

#ifndef ndnrtc_webrtc_h
#define ndnrtc_webrtc_h


#include <interfaces.hpp>

#include "ndnrtc-common.hpp"
// #include <common_types.h>
// #include <api/video/video_frame.h>
// #include <api/video/i420_buffer.h>

typedef struct _VideoFrame {
  unsigned int width() const { return 0; }
  unsigned int height() const { return 0; }
} VideoFrame;

// typedef enum _FrameType {
//   Key,
//   Delta
// } FrameType;

typedef struct _I420Buffer {
  void Create(){};
} I420Buffer;

typedef _VideoFrame WebRtcVideoFrame; // TODO: rename
typedef FrameType WebRtcVideoFrameType;  // TODO: rename
typedef _I420Buffer WebRtcVideoFrameBuffer;  // TODO: rename

 // TODO: rename
template<typename T>
using WebRtcSmartPtr = std::shared_ptr<T>;

// TODO: rename
namespace webrtc {
  class EncodedImage {
  public:
    unsigned int _length;
    uint8_t *_buffer;
    unsigned int _encodedWidth, _encodedHeight;
    uint64_t _timeStamp, capture_time_ms_;
    FrameType _frameType;
    bool _completeFrame;
  };

  typedef int VideoType;

  enum FrameFormat {
    kI420,
    kBGRA,
    kARGB
  };

  enum VideoFrameType {
    kVideoFrameKey
  };

  int CalcBufferSize(FrameFormat, unsigned int, unsigned int);
  // {
  //   return 0;
  // }

  typedef std::function<void()> DecodedImageCallback;
  typedef std::function<void()> EncodedImageCallback;

  typedef int VideoCodec;
}

// typedef int FrameInfo;

void ConvertFromI420(WebRtcVideoFrame, webrtc::VideoType, int, uint8_t*);

#endif
