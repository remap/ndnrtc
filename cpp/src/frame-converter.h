// 
// frame-converter.h
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "webrtc.h"

namespace ndnrtc {
	typedef struct _ArgbRawFrameWrapper {
		const unsigned int width_;
        const unsigned int height_;
        unsigned char* argbFrameData_;
        unsigned int frameSize_;
	} ArgbRawFrameWrapper;

	typedef struct _I420RawFrameWrapper {
		const unsigned int width_;
		const unsigned int height_;
		const unsigned int strideY_;
		const unsigned int strideU_;
		const unsigned int strideV_;
		const unsigned char* yBuffer_;
		const unsigned char* uBuffer_;
		const unsigned char* vBuffer_;
	} I420RawFrameWrapper;

	class RawFrameConverter 
	{
	public:
		RawFrameConverter(){}
		~RawFrameConverter(){}

		WebRtcVideoFrame& operator<<(const ArgbRawFrameWrapper&);
		WebRtcVideoFrame& operator<<(const I420RawFrameWrapper&);

	private:
		WebRtcVideoFrame convertedFrame_;
	};
}