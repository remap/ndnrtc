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

#ifndef ndnrtc_webrtc_h
#define ndnrtc_webrtc_h

#include <webrtc/common_types.h>
#include <webrtc/api/video/video_frame.h>
#include <webrtc/api/video/i420_buffer.h>

typedef webrtc::VideoFrame WebRtcVideoFrame;
typedef webrtc::FrameType WebRtcVideoFrameType;
typedef webrtc::I420Buffer WebRtcVideoFrameBuffer;

template<typename T>
using WebRtcSmartPtr = rtc::scoped_refptr<T>;

#endif
