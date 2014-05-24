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
    int res = Playout::start();
    
    validGop_ = false;
    currentKeyNo_ = 0;
    
    return res;
}

//******************************************************************************
#pragma mark - private
bool
VideoPlayout::playbackPacket(int64_t packetTsLocal, PacketData* data,
                             PacketNumber playbackPacketNo,
                             PacketNumber sequencePacketNo,
                             PacketNumber pairedPacketNo,
                             bool isKey, double assembledLevel)
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

        bool pushFrameFurther = false;
        
        if (params_.skipIncomplete)
        {
            if (isKey)
            {
                if (assembledLevel >= 1)
                {
                    pushFrameFurther = true;
                    currentKeyNo_ = sequencePacketNo;
                    validGop_ = true;
                    LogTraceC << "new GOP with key: "
                    << sequencePacketNo << endl;
                }
                else
                {
                    validGop_ = false;
                    LogTraceC << "GOP failed - key incomplete: "
                    << sequencePacketNo << endl;
                }
            }
            else
            {
                if (pairedPacketNo != currentKeyNo_)
                    LogTraceC
                    << playbackPacketNo << " is unexpected: "
                    << " current key " << currentKeyNo_
                    << " got " << pairedPacketNo << endl;
                
                validGop_ &= (assembledLevel >= 1);
                pushFrameFurther = validGop_ && (pairedPacketNo == currentKeyNo_);
            }
        }
        else
            pushFrameFurther  = true;
        
        if (pushFrameFurther)
        {
            LogStatC << "\tplay\t" << playbackPacketNo << "\ttotal\t" << nPlayed_ << endl;
            ((IEncodedFrameConsumer*)frameConsumer_)->onEncodedFrameDelivered(frame, NdnRtcUtils::unixTimestamp());
        }
        else
        {
            LogWarnC << "skipping incomplete/out of order frame " << playbackPacketNo
            << " isKey: " << (isKey?"YES":"NO")
            << " level: " << assembledLevel << endl;
            nMissed_++;
        }
        
        res = true;
    } // if data
    
    return res;
}