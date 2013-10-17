//
//  receiver-channel.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "receiver-channel.h"

using namespace ndnrtc;
using namespace webrtc;
using namespace std;

//********************************************************************************
#pragma mark - construction/destruction
NdnReceiverChannel::NdnReceiverChannel(NdnParams *params) : NdnRtcObject(params),
localRender_(new NdnRenderer(1,params)),
decoder_(new NdnVideoDecoder(params)),
receiver_(new NdnVideoReceiver(params))
{
    localRender_->setObserver(this);
    decoder_->setObserver(this);
    receiver_->setObserver(this);
    
    receiver_->setFrameConsumer(decoder_.get());
    decoder_->setFrameConsumer(localRender_.get());
}

void NdnReceiverChannel::onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, ndn::Transport& transport)
{
    INFO("got interest: %s", interest->getName().toUri().c_str());
}

void NdnReceiverChannel::onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    ERR("failed to register prefix %s", prefix->toUri().c_str());
}

int NdnReceiverChannel::init()
{
    // connect to ndn
    try
    {
        std::string host = getParams()->getConnectHost();
        int port = getParams()->getConnectPort();
        
        if (host != "" && port > 0)
        {
            shared_ptr<ndn::Transport::ConnectionInfo> connInfo(new TcpTransport::ConnectionInfo(host.c_str(), port));
            
            ndnTransport_.reset(new TcpTransport());
            ndnFace_.reset(new Face(ndnTransport_, connInfo));
            
            std::string streamAccessPrefix = ((VideoReceiverParams*)getParams())->getStreamKeyPrefix();
            ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                     bind(&NdnReceiverChannel::onInterest, this, _1, _2, _3),
                                     bind(&NdnReceiverChannel::onRegisterFailed, this, _1));
        }
        else
            return notifyError(-1, "malformed parameters for host/port");
    }
    catch (std::exception &e)
    {
        return notifyError(-1, "got error form ndn library: %s", e.what());
    }
    
    if (localRender_->init() < 0)
        notifyError(-1, "can't intialize renderer");
    
    if (decoder_->init() < 0)
        notifyError(-1, "can't intialize video encoder");
    
    if (receiver_->init(ndnFace_) < 0)
        return notifyError(-1, "can't intialize video sender");
    
    return 0;
}
int NdnReceiverChannel::startFetching()
{
    TRACE("stopping transmission");
    
    unsigned int tid = 1;
    
    if (localRender_->startRendering() < 0)
        return notifyError(-1, "can't start render");
    
    if (receiver_->startFetching() < 0)
        return notifyError(-1, "can't start fetching frames");
    
    isTransmitting_ = true;
    return 0;
}
int NdnReceiverChannel::stopFetching()
{

    receiver_->stopFetching();
    
    isTransmitting_ = false;
    
    return 0;
}
void NdnReceiverChannel::getStat(ReceiverChannelStatistics &stat) const
{
    memset(&stat,0,sizeof(stat));
    
    stat.nPipeline_ = receiver_->getNPipelined();
    stat.nPlayback_ = receiver_->getNPlayout();
//    stat.nFetched_ = receiver_->getLatest(FrameBuffer::Slot::StateReady);
    stat.nLate_ = receiver_->getNLateFrames();
    
    stat.nSkipped_ = receiver_->getPlaybackSkipped();
//    stat.nTotalTimeouts_ = receiver_
//    stat.nTimeouts_ = receiver_
    
    stat.nFree_ = receiver_->getBufferStat(FrameBuffer::Slot::StateFree);
    stat.nLocked_ = receiver_->getBufferStat(FrameBuffer::Slot::StateLocked);
    stat.nAssembling_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    stat.nNew_ = receiver_->getBufferStat(FrameBuffer::Slot::StateAssembling);
    
    stat.playoutFreq_  = receiver_->getPlayoutFreq();
    stat.inDataFreq_ = receiver_->getIncomeDataFreq();
    stat.inFramesFreq_ = receiver_->getIncomeFramesFreq();
}
