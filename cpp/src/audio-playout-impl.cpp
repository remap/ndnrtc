// 
// audio-playout-impl.cpp
//
//  Created by Peter Gusev on 03 August 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "audio-playout-impl.hpp"
#include "frame-data.hpp"
#include "frame-buffer.hpp"
#include "audio-renderer.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace webrtc;

AudioPlayoutImpl::AudioPlayoutImpl(boost::asio::io_service& io,
            const std::shared_ptr<IPlaybackQueue>& queue,
            const std::shared_ptr<StatStorage>& statStorage,
            const WebrtcAudioChannel::Codec& codec,
            unsigned int deviceIdx):
PlayoutImpl(io, queue, statStorage), packetCount_(0),
renderer_(std::make_shared<AudioRenderer>(deviceIdx, codec))
{
    description_ = "aplayout";
}

void AudioPlayoutImpl::start(unsigned int fastForwardMs)
{
    PlayoutImpl::start(fastForwardMs);
    renderer_->startRendering();
}

void AudioPlayoutImpl::stop()
{
    PlayoutImpl::stop();
    packetCount_ = 0;
    renderer_->stopRendering();
}

void AudioPlayoutImpl::setLogger(std::shared_ptr<ndnlog::new_api::Logger> logger)
{
    PlayoutImpl::setLogger(logger);
    renderer_->setLogger(logger);
}

bool AudioPlayoutImpl::processSample(const std::shared_ptr<const BufferSlot>& slot)
{
    std::shared_ptr<ImmutableAudioBundlePacket> bundlePacket = 
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
        return true;
    }
    else
        LogWarnC << "Error reading audio bundle " << slot->dump() << std::endl;

    return false;
}
