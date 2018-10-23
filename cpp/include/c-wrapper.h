// 
// c-wrapper.h
//
// Copyright (c) 2017 Regents of the University of California
// For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __c_wrapper_h__
#define __c_wrapper_h__

#include "local-stream.hpp"
#include "remote-stream.hpp"

extern "C" {

    typedef void (*LibLog) (const char* message);
    const char* ndnrtc_getVersion();

	void ndnrtc_get_identities_list(char*** identities, int* nIdentities);

	// bool ndnrtc_check_nfd_connection(const char* hostname, int port);

	// creates Face
	//		hostname
	//		portnum
	// creates KeyChain
	//		optional: local key storage
	// - runs everything on internal thread
	bool ndnrtc_init(const char* hostname, const char* storagePath, 
		const char* policyFilePath, const char* signingIdentity, 
		const char * instanceId, LibLog libLog);

	// deinitializes library (removes connection and frees objects)
	// init can be called again after this
	void ndnrtc_deinit();


    typedef struct _LocalStreamParams {
        const char *basePrefix;
        int signingOn;
        int fecOn;
        int typeIsVideo;
        int ndnSegmentSize;
        int frameWidth, frameHeight;
        int startBitrate, maxBitrate, gop, dropFrames;
        const char *streamName, *threadName;
        const char *storagePath; 
    } LocalStreamParams;

    typedef struct _FrameInfo {
        uint64_t timestamp_;
        int playbackNo_;
        char* ndnName_;
    } cFrameInfo;

    typedef unsigned char* (*BufferAlloc) (const char* frameName, 
                                           int width, int height);
    typedef void (*FrameFetched) (const cFrameInfo finfo, int width, int height, 
                                  const unsigned char* buffer);

	// params
	//	base prefix
	//	settings
	//		sign
	//		faceIO
	//		keychain
	//		face
	//		params
	//			stream name
	//			producer params
	//				segment size
	//				freshness
	//		coder
	//			frame_rate
	//			gop
	//			start_bitrate
	//			max_bitrate
	//			encode_height
	//			encode_width
	//			drop_frames
	//	use FEC
	ndnrtc::IStream* ndnrtc_createLocalStream(LocalStreamParams params, LibLog loggerSink);
	void ndnrtc_destroyLocalStream(ndnrtc::IStream* localStreamObject);

	int ndnrtc_LocalVideoStream_incomingI420Frame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideU,
			const unsigned int strideV,
			const unsigned char* yBuffer,
			const unsigned char* uBuffer,
			const unsigned char* vBuffer);

	int ndnrtc_LocalVideoStream_incomingNV21Frame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			const unsigned int strideY,
			const unsigned int strideUV,
			const unsigned char* yBuffer,
			const unsigned char* uvBuffer);

	int ndnrtc_LocalVideoStream_incomingArgbFrame(ndnrtc::LocalVideoStream *stream,
			const unsigned int width,
			const unsigned int height,
			unsigned char* argbFrameData,
			unsigned int frameSize);

	typedef struct _RemoteStreamParams {
		const char *basePrefix;
		int jitterSizeMs, lifetimeMs;
		const char *streamName;
	} RemoteStreamParams;

	// creates remote video stream
	ndnrtc::IStream* ndnrtc_createRemoteStream(RemoteStreamParams params, LibLog loggerSink);
	void ndnrtc_destroyRemoteStream(ndnrtc::IStream* remoteStreamObject);

    // NOTE: returns info only for 1 thread
    cFrameInfo ndnrtc_LocalVideoStream_getLastPublishedInfo(ndnrtc::LocalVideoStream *stream);

	const char* ndnrtc_LocalStream_getPrefix(ndnrtc::IStream *stream);
	const char* ndnrtc_LocalStream_getBasePrefix(ndnrtc::IStream *stream);
	const char* ndnrtc_LocalStream_getStreamName(ndnrtc::IStream *stream);

    // see statistics.cpp for possible values
    double ndnrtc_getStatistic(ndnrtc::IStream *stream, const char* statName);

    // fetch frame from local storage of the local stream
    void ndnrtc_FrameFetcher_fetch(ndnrtc::IStream *stream,
                                   const char* frameName, 
                                   BufferAlloc bufferAllocFunc,
                                   FrameFetched frameFetchedFunc);
}

#endif