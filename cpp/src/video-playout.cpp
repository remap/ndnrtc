//
//  video-playout.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "video-playout.h"
#include "video-consumer.h"

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
VideoPlayout::VideoPlayout(const Consumer* consumer):
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
    
    // render frame if we have one
    if (frameConsumer_)
    { 
        bool pushFrameFurther = false;
        
        if (consumer_->getGeneralParameters().skipIncomplete_)
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
                    stat_.nSkippedIncomplete_++;
                    stat_.nSkippedIncompleteKey_++;
                    
                    getVideoConsumer()->playbackEventOccurred(PlaybackEventKeySkipIncomplete,
                                                              sequencePacketNo);
                    
                    LogTraceC << "GOP failed - key incomplete ("
                    << assembledLevel << "): "
                    << sequencePacketNo << endl;
                }
            }
            else
            {
                // update stat
                if (assembledLevel < 1.)
                {
                    stat_.nSkippedIncomplete_++;
                    
                    getVideoConsumer()->playbackEventOccurred(PlaybackEventDeltaSkipIncomplete,
                                                              sequencePacketNo);
                    
                    LogTraceC
                    << playbackPacketNo << " incomplete "
                    << assembledLevel << endl;
                }
                else
                {
                    if (pairedPacketNo != currentKeyNo_)
                    {
                        stat_.nSkippedNoKey_++;
                        
                        getVideoConsumer()->playbackEventOccurred(PlaybackEventDeltaSkipNoKey, sequencePacketNo);
                        
                        LogTraceC
                        << playbackPacketNo << " is unexpected: "
                        << " current key " << currentKeyNo_
                        << " got " << pairedPacketNo << endl;
                    }
                    else
                    {
                        if (!validGop_)
                        {
                            stat_.nSkippedInvalidGop_++;
                            
                            getVideoConsumer()->playbackEventOccurred(PlaybackEventDeltaSkipInvalidGop, sequencePacketNo);
                            
                            LogTraceC
                            << playbackPacketNo << " bad GOP " << endl;
                        }
                    }
                }
                
                validGop_ &= (assembledLevel >= 1);
                pushFrameFurther = validGop_ && (pairedPacketNo == currentKeyNo_);
            }
        }
        else
            pushFrameFurther  = true;
        
        // check for valid data
        pushFrameFurther &= (data_ != NULL);
        
        if (pushFrameFurther)
        {
            webrtc::EncodedImage frame;
            
            ((NdnFrameData*)data)->getFrame(frame);
            
#if RECORD
            PacketData::PacketMetadata meta = data_->getMetadata();
            frameWriter.writeFrame(frame, meta);
#endif
            
            // update stat
            stat_.nPlayed_++;
            if (isKey)
                stat_.nPlayedKey_++;
            
            LogStatC << "\tplay\t" << playbackPacketNo << "\ttotal\t" << stat_.nPlayed_ << endl;
            ((IEncodedFrameConsumer*)frameConsumer_)->onEncodedFrameDelivered(frame, NdnRtcUtils::unixTimestamp());
        }
        else
        {
            LogWarnC << "skipping incomplete/out of order frame " << playbackPacketNo
            << " isKey: " << (isKey?"YES":"NO")
            << " level: " << assembledLevel << endl;
        }
        
        res = pushFrameFurther;
    } // if data
    
    return res;
}