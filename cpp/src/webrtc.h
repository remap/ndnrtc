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

#include <video_engine/vie_impl.h>

// other
#include <common_types.h>
#include <system_wrappers/interface/event_wrapper.h>
#include <system_wrappers/interface/thread_wrapper.h>
#include <system_wrappers/interface/critical_section_wrapper.h>
#include <system_wrappers/interface/tick_util.h>
#include <system_wrappers/interface/rw_lock_wrapper.h>

// video capturing
#include <modules/video_capture/include/video_capture_defines.h>
#include <modules/video_capture/include/video_capture.h>
//#include <modules/video_capture/mac//qtkitvideo_capture.h>
#include <modules/video_capture/include/video_capture_factory.h>
#include <common_video/libyuv/include/webrtc_libyuv.h>

// codecs
#include <modules/video_coding/codecs/vp8/include/vp8.h>
#include <modules/video_coding/main/interface/video_coding_defines.h>
#include <modules/video_coding/main/source/codec_database.h>
#include <modules/video_coding/main/source/internal_defines.h>

// audio
#include <voice_engine/include/voe_base.h>
#include <voice_engine/include/voe_network.h>
#include <voice_engine/include/voe_audio_processing.h>
#include <modules/audio_coding/main/interface/audio_coding_module.h>

#endif
