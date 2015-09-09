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
using namespace ndnrtc::new_api::statistics;

#define RECORD 0
#if RECORD
#include "ndnrtc-testing.h"
using namespace ndnrtc::testing;
static EncodedFrameWriter frameWriter("received.nrtc");
#endif

//******************************************************************************
#pragma mark - construction/destruction
VideoPlayout::VideoPlayout(Consumer* consumer,
                           const boost::shared_ptr<statistics::StatisticsStorage>& statStorage):
Playout(consumer, statStorage)
{
    description_ = "video-playout";
}

VideoPlayout::~VideoPlayout()
{
    
}

//******************************************************************************
#pragma mark - public
int
VideoPlayout::start(int playbackAdjustment)
{
    validGop_ = false;
    currentKeyNo_ = 0;
    
    int res = Playout::start(playbackAdjustment);
    
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
                    << currentKeyNo_ << endl;
                }
                else
                {
                    validGop_ = false;
                    (*statStorage_)[Indicator::SkippedIncompleteNum]++;
                    (*statStorage_)[Indicator::SkippedIncompleteKeyNum]++;
                    
                    getVideoConsumer()->playbackEventOccurred(PlaybackEventKeySkipIncomplete,
                                                              sequencePacketNo);
                    
                    LogWarnC << "key incomplete."
                    << " lvl " << assembledLevel
                    << " seq " << sequencePacketNo
                    << " abs " << playbackPacketNo
                    << endl;
                }
            }
            else
            {
                // update stat
                if (assembledLevel < 1.)
                {
                    (*statStorage_)[Indicator::SkippedIncompleteNum]++;
                    
                    getVideoConsumer()->playbackEventOccurred(PlaybackEventDeltaSkipIncomplete,
                                                              sequencePacketNo);
                    
                    LogWarnC << "delta incomplete."
                    << " lvl " << assembledLevel
                    << " seq " << sequencePacketNo
                    << " abs " << playbackPacketNo
                    << endl;
                }
                else
                {
                    if (pairedPacketNo != currentKeyNo_)
                    {
                        (*statStorage_)[Indicator::SkippedNoKeyNum]++;
                        
                        getVideoConsumer()->playbackEventOccurred(PlaybackEventDeltaSkipNoKey, sequencePacketNo);
                        
                        LogWarnC << "delta gop mismatch."
                        << " current " << currentKeyNo_
                        << " received " << pairedPacketNo
                        << " seq " << sequencePacketNo
                        << " abs " << playbackPacketNo
                        << endl;
                    }
                    else
                    {
                        if (!validGop_)
                        {
                            (*statStorage_)[Indicator::SkippedBadGopNum]++;
                            
                            getVideoConsumer()->playbackEventOccurred(PlaybackEventDeltaSkipInvalidGop, sequencePacketNo);
                            
                            LogWarnC << "invalid gop."
                            << " seq " << sequencePacketNo
                            << " abs " << playbackPacketNo
                            << " key " << currentKeyNo_
                            << endl;
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
        pushFrameFurther &= (data != NULL);
        
#if RECORD
        if (pushFrameFurther)
        {
            PacketData::PacketMetadata meta = data_->getMetadata();
            frameWriter.writeFrame(frame, meta);
        }
#endif
        
        if (!pushFrameFurther)
        {
            if (onFrameSkipped_)
                onFrameSkipped_(playbackPacketNo, sequencePacketNo,
                                pairedPacketNo, isKey, assembledLevel);
            
            LogDebugC << "bad frame."
            << " type " << (isKey?"K":"D")
            << " lvl " << assembledLevel
            << " seq " << sequencePacketNo
            << " abs " << playbackPacketNo
            << endl;
        }
        else
        {
            LogDebugC << "playback."
            << " type " << (isKey?"K":"D")
            << " seq " << sequencePacketNo
            << " abs " << playbackPacketNo
            << " total " << (*statStorage_)[Indicator::PlayedNum]
            << endl;
        
            webrtc::EncodedImage frame;
            
            if (data)
                ((NdnFrameData*)data)->getFrame(frame);
            
            // update stat
            (*statStorage_)[Indicator::PlayedNum]++;
            if (isKey)
                (*statStorage_)[Indicator::PlayedKeyNum]++;
        
            ((IEncodedFrameConsumer*)frameConsumer_)->onEncodedFrameDelivered(frame, NdnRtcUtils::unixTimestamp(), pushFrameFurther);
        }

        res = pushFrameFurther;
    } // if data
    
    return res;
}