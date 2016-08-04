// 
// audio-playout-impl.cpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "audio-playout-impl.h"
#include "frame-data.h"
#include "frame-buffer.h"
#include "audio-renderer.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace webrtc;

AudioPlayoutImpl::AudioPlayoutImpl(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage>& statStorage):
PlayoutImpl(io, queue, statStorage), packetCount_(0)
{
    description_ = "aplayout";
}

void AudioPlayoutImpl::start(unsigned int devIdx, WebrtcAudioChannel::Codec codec)
{
    renderer_ = boost::make_shared<AudioRenderer>(devIdx, codec);
    renderer_->setLogger(logger_);

    PlayoutImpl::start();
    renderer_->startRendering();
}

void AudioPlayoutImpl::stop()
{
    PlayoutImpl::stop();
    packetCount_ = 0;
    renderer_->stopRendering();
}

void AudioPlayoutImpl::processSample(const boost::shared_ptr<const BufferSlot>& slot)
{
    boost::shared_ptr<ImmutableAudioBundlePacket> bundlePacket = 
        bundleSlot_.readBundle(*slot);
    
    if (bundlePacket.get())
    {
        for (int i = 0; i < bundlePacket->getSamplesNum(); ++i)
        {
            ImmutableAudioBundlePacket::AudioSampleBlob 
                sampleBlob = (*bundlePacket)[i];
            if (sampleBlob.getHeader().isRtcp_)
                renderer_->onDeliverRtcpFrame(sampleBlob.payloadLength(), (unsigned char*)sampleBlob.data());
            else
                renderer_->onDeliverRtpFrame(sampleBlob.payloadLength(), (unsigned char*)sampleBlob.data());
        }            
    }
    else
        LogWarnC << "Error reading audio bundle " << slot->dump() << std::endl;
}
