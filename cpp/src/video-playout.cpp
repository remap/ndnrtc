//
//  video-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "video-playout.h"

using namespace std;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;

#define RECORD 0
#if RECORD
#include "ndnrtc-testing.h"
using namespace ndnrtc::testing;
static EncodedFrameWriter frameWriter("received.nrtc");
#endif

//******************************************************************************
#pragma mark - construction/destruction
VideoPlayout::VideoPlayout(const shared_ptr<const Consumer>& consumer):
Playout(consumer)
{
    description_ = "video-playout";
}

VideoPlayout::~VideoPlayout()
{
    
}

//******************************************************************************
#pragma mark - public
int
VideoPlayout::start()
{
    Playout::start();
    hasKeyForGop_ = false;
}

//******************************************************************************
#pragma mark - private
bool
VideoPlayout::playbackPacket(int64_t packetTsLocal, PacketData* data,
                             PacketNumber packetNo, bool isKey,
                             double assembledLevel)
{
    bool res = false;
    webrtc::EncodedImage frame;
    
    // render frame if we have one
    if (data && frameConsumer_)
    { 
        ((NdnFrameData*)data)->getFrame(frame);
        
#if RECORD
        PacketData::PacketMetadata meta = data_->getMetadata();
        frameWriter.writeFrame(frame, meta);
#endif

        if (params_.skipIncomplete)
        {
            if (isKey && assembledLevel >= 1)
            {
                hasKeyForGop_ = true;
                LogTraceC << "resumed frames playout" << endl;
            }
            
            if (assembledLevel < 1)
                hasKeyForGop_ = false;
        }
        
        if (hasKeyForGop_ || !params_.skipIncomplete)
            ((IEncodedFrameConsumer*)frameConsumer_)->onEncodedFrameDelivered(frame);
        else
            LogWarnC << "skipping incomplete frame " << packetNo
            << " isKey: " << (isKey?"YES":"NO")
            << " level: " << assembledLevel << endl;
        
        res = true;
    }
    
    return res;
}