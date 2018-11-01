// 
// frame-converter.hpp
//
//  Created by Peter Gusev on 18 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "webrtc.hpp"

namespace ndnrtc {
	struct _8bitFixedSizeRawFrameWrapper {
		const unsigned int width_;
        const unsigned int height_;
        unsigned char* frameData_;
        unsigned int frameSize_;
        bool isArgb_;
	};

    typedef _8bitFixedSizeRawFrameWrapper ArgbRawFrameWrapper;
    typedef _8bitFixedSizeRawFrameWrapper BgraRawFrameWrapper;

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

	typedef struct _YUV_NV21FrameWrapper {
		const unsigned int width_;
		const unsigned int height_;
		const unsigned int strideY_;
		const unsigned int strideUV_;
		const unsigned char* yBuffer_;
		const unsigned char* uvBuffer_;
	} YUV_NV21FrameWrapper; 

	/**
	 * FrameConverter converts wrappers of raw video frames into a
	 * WebRTC raw video frame object. Converted object is stored inside the
	 * converter and is valid as long as converter lives.
	 */
	class RawFrameConverter 
	{
	public:
		RawFrameConverter(){}
		~RawFrameConverter(){}

		WebRtcVideoFrame operator<<(const struct _8bitFixedSizeRawFrameWrapper&);
		WebRtcVideoFrame operator<<(const I420RawFrameWrapper&);
		WebRtcVideoFrame operator<<(const YUV_NV21FrameWrapper&);

	private:
		WebRtcSmartPtr<WebRtcVideoFrameBuffer> frameBuffer_;

        WebRtcVideoFrame convert(const struct _8bitFixedSizeRawFrameWrapper&, 
                                 const webrtc::VideoType&);
	};
}