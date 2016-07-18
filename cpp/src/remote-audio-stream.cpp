// 
// remote-audio-stream.cpp
//
//  Created by Peter Gusev on 30 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "remote-audio-stream.h"

using namespace ndnrtc;

RemoteAudioStreamImpl::RemoteAudioStreamImpl(boost::asio::io_service& io, 
			const boost::shared_ptr<ndn::Face>& face,
			const boost::shared_ptr<ndn::KeyChain>& keyChain,
			const std::string& streamPrefix):
RemoteStreamImpl(io, face, keyChain, streamPrefix)
{
    type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeAudio;
}

void 
RemoteAudioStreamImpl::initiateFetching()
{

}
