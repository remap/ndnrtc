// 
// remote-video-stream.hpp
//
//  Created by Peter Gusev on 29 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __remote_video_stream_h__
#define __remote_video_stream_h__

#include "remote-stream-impl.hpp"
#include "sample-validator.hpp"
#include "webrtc.hpp"

namespace ndnrtc{
	class IPlayoutControl;
	class VideoPlayout;
	class PipelineControl;
	class ManifestValidator;
	class VideoDecoder;
	class IExternalRenderer;

	class RemoteVideoStreamImpl : public RemoteStreamImpl
	{
	public:
		RemoteVideoStreamImpl(boost::asio::io_service& io, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix);
		~RemoteVideoStreamImpl();
		
		void start(const std::string& threadName, IExternalRenderer* render);
		void initiateFetching();
        void stopFetching();
        void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);
        
	private:
		boost::shared_ptr<ManifestValidator> validator_;
		IExternalRenderer* renderer_;
		boost::shared_ptr<VideoDecoder> decoder_;

		void feedFrame(const WebRtcVideoFrame&);
        void setupDecoder();
        void releaseDecoder();
        void setupPipelineControl();
        void releasePipelineControl();
	};
}

#endif
