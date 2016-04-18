// 
// tests-helpers.cc
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "tests-helpers.h"

using namespace std;
using namespace ndnrtc;

VideoCoderParams sampleVideoCoderParams()
{
	VideoCoderParams vcp;

	vcp.codecFrameRate_ = 30;
	vcp.gop_ = 30;
	vcp.startBitrate_ = 1000;
	vcp.maxBitrate_ = 3000;
	vcp.encodeWidth_ = 1920;
	vcp.encodeHeight_ = 1080;
	vcp.dropFramesOn_ = true;

	return vcp;
}

ClientParams sampleConsumerParams()
{
	ClientParams cp;

	{
		GeneralParams gp;

		gp.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
		gp.logFile_ = "ndnrtc.log";
		gp.logPath_ = "/tmp";
		gp.useRtx_ = true;
		gp.useFec_ = false;
		gp.useAvSync_ = true;
		gp.skipIncomplete_ = true;
		gp.host_ = "aleph.ndn.ucla.edu";
		gp.portNum_ = 6363;
		cp.setGeneralParameters(gp);
	}

	{
		ConsumerStreamParams msp1;

		msp1.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
		msp1.streamSink_ = "mic.pcmu";
		msp1.threadToFetch_ = "pcmu";
		msp1.streamName_ = "mic";
		msp1.type_ = MediaStreamParams::MediaStreamTypeAudio;
		msp1.synchronizedStreamName_ = "camera";
		msp1.producerParams_.freshnessMs_ = 2000;
		msp1.producerParams_.segmentSize_ = 1000;

		ConsumerStreamParams msp2;

		msp2.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
		msp2.streamSink_ = "camera.yuv";
		msp2.threadToFetch_ = "low";
		msp2.streamName_ = "camera";
		msp2.type_ = MediaStreamParams::MediaStreamTypeVideo;
		msp2.synchronizedStreamName_ = "mic";
		msp2.producerParams_.freshnessMs_ = 2000;
		msp2.producerParams_.segmentSize_ = 1000;

		GeneralConsumerParams gcpa;

		gcpa.interestLifetime_ = 2000;
		gcpa.bufferSlotsNum_ = 150;
		gcpa.slotSize_ = 8000;
		gcpa.jitterSizeMs_ = 150;

		GeneralConsumerParams gcpv;

		gcpv.interestLifetime_ = 2000;
		gcpv.bufferSlotsNum_ = 200;
		gcpv.slotSize_ = 16000;
		gcpv.jitterSizeMs_ = 150;

		StatGatheringParams sgp("buffer");
		static const char* s[] = {"jitterPlay", "jitterTar", "drdPrime"};
		vector<string> v(s, s+3);
		sgp.addStats(v);

		ConsumerClientParams ccp;
		ccp.generalAudioParams_ = gcpa;
		ccp.generalVideoParams_ = gcpv;
		ccp.statGatheringParams_.push_back(sgp);

		ccp.fetchedStreams_.push_back(msp1);
		ccp.fetchedStreams_.push_back(msp2);

		cp.setConsumerParams(ccp);
	}

	return cp;
}

ClientParams sampleProducerParams()
{
	ClientParams cp;

	{
		GeneralParams gp;

		gp.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
		gp.logFile_ = "ndnrtc.log";
		gp.logPath_ = "/tmp";
		gp.useRtx_ = true;
		gp.useFec_ = false;
		gp.useAvSync_ = true;
		gp.skipIncomplete_ = true;
		gp.host_ = "aleph.ndn.ucla.edu";
		gp.portNum_ = 6363;
		cp.setGeneralParameters(gp);
	}

	{
		ProducerClientParams pcp;

		pcp.username_ = "client1";
		pcp.prefix_ = "/ndn/edu/ucla/remap";

		{
			ProducerStreamParams msp;
			stringstream ss;
	
			msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
			msp.streamName_ = "mic";
			msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
			msp.producerParams_.freshnessMs_ = 2000;
			msp.producerParams_.segmentSize_ = 1000;
	
			CaptureDeviceParams cdp;
			cdp.deviceId_ = 10;
			msp.captureDevice_ = cdp;
			msp.addMediaThread(AudioThreadParams("pcmu"));
			msp.addMediaThread(AudioThreadParams("g722"));
	
			pcp.publishedStreams_.push_back(msp);
		}

		{
			ProducerStreamParams msp;
			stringstream ss;
	
			msp.sessionPrefix_ = "/ndn/edu/ucla/remap/ndnrtc/user/client1";
			msp.streamName_ = "camera";
			msp.source_ = "/tmp/camera.argb";
			msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
			msp.synchronizedStreamName_ = "mic";
			msp.producerParams_.freshnessMs_ = 2000;
			msp.producerParams_.segmentSize_ = 1000;
	
			CaptureDeviceParams cdp;
			cdp.deviceId_ = 11;
			msp.captureDevice_ = cdp;
			msp.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
			msp.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

			pcp.publishedStreams_.push_back(msp);
		}

		cp.setProducerParams(pcp);
	}

	return cp;
}