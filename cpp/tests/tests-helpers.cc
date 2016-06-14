// 
// tests-helpers.cc
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/assign.hpp>
#include <ndn-cpp/interest.hpp>

#include "tests-helpers.h"

using namespace std;
using namespace ndnrtc;
using namespace ndn;

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

webrtc::EncodedImage encodedImage(size_t frameLen, uint8_t*& buffer, bool delta)
{
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    buffer = (uint8_t*)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = (delta ? webrtc::kDeltaFrame : webrtc::kKeyFrame);
    frame._completeFrame = true;

    return frame;
}

bool checkVideoFrame(const webrtc::EncodedImage& image)
{
	bool identical = true;
	uint8_t *buf;
	webrtc::EncodedImage ref = encodedImage(image._length, buf, (image._frameType == webrtc::kDeltaFrame));

	identical = (image._encodedWidth == ref._encodedWidth) &&
		(image._encodedHeight == ref._encodedHeight) &&
		(image._timeStamp == ref._timeStamp) &&
		(image.capture_time_ms_ == ref.capture_time_ms_) &&
		(image._frameType == ref._frameType) &&
		(image._completeFrame == ref._completeFrame) &&
		(image._length == ref._length);

	for (int i = 0; i < image._length && identical; ++i)
		identical &= (image._buffer[i] == ref._buffer[i]);

	free(buf);
	return identical;
}

VideoFramePacket getVideoFramePacket(size_t frameLen, double rate, int64_t pubTs,
	int64_t pubUts)
{
 	CommonHeader hdr;
    hdr.sampleRate_ = rate;
    hdr.publishTimestampMs_ = pubTs;
    hdr.publishUnixTimestampMs_ = pubUts;

    uint8_t *buffer;

    webrtc::EncodedImage frame = encodedImage(frameLen, buffer);

    VideoFramePacket vp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

    vp.setSyncList(syncList);
    vp.setHeader(hdr);
    
    free(buffer);

    return vp;
}

std::vector<VideoFrameSegment> sliceFrame(VideoFramePacket& vp, 
	PacketNumber playNo, PacketNumber pairedSeqNo)
{
    boost::shared_ptr<NetworkData> parity = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);

    std::vector<VideoFrameSegment> frameSegments = VideoFrameSegment::slice(vp, 1000);
    std::vector<CommonSegment> paritySegments = CommonSegment::slice(*parity, 1000);

    // pack segments into data objects
    int idx = 0;
    VideoFrameSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;
    header.totalSegmentsNum_ = frameSegments.size();
    header.playbackNo_ = playNo;
    header.pairedSequenceNo_ = pairedSeqNo;
    header.paritySegmentsNum_ = paritySegments.size();

    for (auto& s:frameSegments)
    {
        VideoFrameSegmentHeader hdr = header;
        hdr.interestNonce_ += idx;
        hdr.interestArrivalMs_ += idx;
        hdr.playbackNo_ += idx;
        idx++;
        s.setHeader(hdr);
    }

    return frameSegments;
}

std::vector<CommonSegment> sliceParity(VideoFramePacket& vp, boost::shared_ptr<NetworkData>& parity)
{
	DataSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;

	parity = vp.getParityData(CommonSegment::payloadLength(1000), 0.2);
	assert(parity.get());

	std::vector<CommonSegment> paritySegments = CommonSegment::slice(*parity, 1000);

	int idx = 0;
	for (auto& s:paritySegments)
	{
		DataSegmentHeader hdr = header;
		hdr.interestNonce_ += idx;
		s.setHeader(hdr);
	}

	return paritySegments;
}

std::vector<boost::shared_ptr<ndn::Data>> 
dataFromSegments(std::string frameName,
	const std::vector<VideoFrameSegment>& segments)
{
    std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
    int segIdx = 0;
    for (auto& s:segments)
    {
        ndn::Name n(frameName);
        n.appendSegment(segIdx++);
        boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
        ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromNumber(segments.size()));
        ds->setContent(s.getNetworkData()->getData(), s.size());
        dataSegments.push_back(ds);
    }

    return dataSegments;
}

std::vector<boost::shared_ptr<ndn::Data>> 
dataFromParitySegments(std::string frameName,
	const std::vector<CommonSegment>& segments)
{
    std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
    int segIdx = 0;
    for (auto& s:segments)
    {
        ndn::Name n(frameName);
        n.append(NameComponents::NameComponentParity).appendSegment(segIdx++);
        boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
        ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromNumber(segments.size()));
        ds->setContent(s.getNetworkData()->getData(), s.size());
        dataSegments.push_back(ds);
    }

    return dataSegments;
}

std::vector<boost::shared_ptr<Interest>>
getInterests(std::string frameName, unsigned int startSeg, size_t nSeg, 
	unsigned int parityStartSeg, size_t parityNSeg, unsigned int startNonce)
{
	std::vector<boost::shared_ptr<Interest>> interests;

	int nonce = startNonce;
	for (int i = startSeg; i < startSeg+nSeg; ++i)
	{
		Name n(frameName);
		n.appendSegment(i);
		interests.push_back(boost::make_shared<Interest>(n, 1000));
		interests.back()->setNonce(Blob((uint8_t*)&(nonce), sizeof(int)));
		nonce++;
	}

	nonce = startNonce;
	for (int i = parityStartSeg; i < parityStartSeg+parityNSeg; ++i)
	{
		Name n(frameName);
		n.append(NameComponents::NameComponentParity).appendSegment(i);
		interests.push_back(boost::make_shared<Interest>(n, 1000));
		interests.back()->setNonce(Blob((uint8_t*)&(nonce), sizeof(int)));
		nonce++;
	}

	return interests;
}

std::vector<boost::shared_ptr<const Interest>> 
makeInterestsConst(const std::vector<boost::shared_ptr<Interest>>& interests)
{
	std::vector<boost::shared_ptr<const Interest>> cinterests;
	for (auto& i:interests)
		cinterests.push_back(boost::make_shared<const Interest>(*i));
	return cinterests;
}

//******************************************************************************
void DelayQueue::push(QueueBlock block)
{
  int d = (dev_ ? std::rand()%(int)(2*dev_)-dev_ : 0);
  int fireDelayMs = delayMs_ + d;
  if (fireDelayMs < 0 ) fireDelayMs = 0;

  TPoint fireTime = Clock::now()+Msec(fireDelayMs);

  {
  	boost::lock_guard<boost::recursive_mutex> scopedLock(m_);
  	queue_[fireTime].push_back(block);
  }

  if (!timerSet_) 
  {
    timerSet_ = true;
    timer_.expires_at(fireTime);
    timer_.async_wait(boost::bind(&DelayQueue::pop, this, 
      boost::asio::placeholders::error));
  }
}

void DelayQueue::reset()
{
  timer_.cancel();
  timerSet_ = false;
}

void DelayQueue::pop(const boost::system::error_code& e)
{
	if (e != boost::asio::error::operation_aborted && isActive())
	{
		boost::lock_guard<boost::recursive_mutex> scopedLock(m_);
		std::vector<QueueBlock> first = queue_.begin()->second;

		for (auto& block:first)
			block();

		if (queue_.size() > 1)
		{
			queue_.erase(queue_.begin());
			timer_.expires_at(queue_.begin()->first);
			timer_.async_wait(boost::bind(&DelayQueue::pop, this, 
				boost::asio::placeholders::error));
		}
		else
		{
			queue_.erase(queue_.begin());
			timerSet_ = false;
		}
	}
}

size_t DelayQueue::size() const 
{
	boost::lock_guard<boost::recursive_mutex> scopedLock(m_);
	return queue_.size();
}

void DataCache::addInterest(const boost::shared_ptr<ndn::Interest> interest, OnDataT onData)
{

  if (data_.find(interest->getName()) != data_.end())
  {
    boost::lock_guard<boost::mutex> scopedLock(m_);
    boost::shared_ptr<ndn::Data> d = data_[interest->getName()];
    
    onData(d, interest);

    if (onInterestCallbacks_.find(interest->getName()) != onInterestCallbacks_.end())
    {
      onInterestCallbacks_[interest->getName()](interest);
      onInterestCallbacks_.erase(onInterestCallbacks_.find(interest->getName()));
    }

    data_.erase(data_.find(interest->getName()));
  }
  else
  {
    boost::lock_guard<boost::mutex> scopedLock(m_);

    interests_[interest->getName()] = interest;
    onDataCallbacks_[interest->getName()] = onData;
  }
}

void DataCache::addData(const boost::shared_ptr<ndn::Data>& data, OnInterestT onInterest)
  {
    if (interests_.find(data->getName()) != interests_.end())
    {
      boost::lock_guard<boost::mutex> scopedLock(m_);
      boost::shared_ptr<ndn::Interest> i = interests_[data->getName()];
      
      if (onInterest) onInterest(i);
      onDataCallbacks_[data->getName()](data, i);

      interests_.erase(interests_.find(data->getName()));
      onDataCallbacks_.erase(onDataCallbacks_.find(data->getName()));
    }
    else
    {
      boost::lock_guard<boost::mutex> scopedLock(m_);

      data_[data->getName()] = data;
    }
  }
