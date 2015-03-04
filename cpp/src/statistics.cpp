//
//  statistics.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author: Peter Gusev
//

#include "statistics.h"

#include <algorithm>
#include <boost/assign.hpp>

using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;
using namespace boost::assign;

const std::map<Indicator, std::string> StatisticsStorage::IndicatorNames =
map_list_of    ( Indicator::AcquiredNum, "Acquired frames" )
( Indicator::AcquiredKeyNum, "Acquired key frames" ) 
( Indicator::DroppedNum, "Dropped frames" ) 
( Indicator::DroppedKeyNum, "Dropped key frames" ) 
( Indicator::AssembledNum, "Assembled frames" ) 
( Indicator::AssembledKeyNum, "Assembled key frames" ) 
( Indicator::RecoveredNum, "Recovered frames" ) 
( Indicator::RecoveredKeyNum, "Recovered key frames" ) 
( Indicator::RescuedNum, "Rescued frames" ) 
( Indicator::RescuedKeyNum, "Rescued key frames" ) 
( Indicator::IncompleteNum, "Incomplete frames" ) 
( Indicator::IncompleteKeyNum, "Incomplete key frames" ) 
( Indicator::BufferTargetSize, "Jitter target size" ) 
( Indicator::BufferPlayableSize, "Jitter playable size" ) 
( Indicator::BufferEstimatedSize, "Jitter estimated size" ) 
( Indicator::CurrentProducerFramerate, "Producer rate" ) 
( Indicator::LastPlayedNo, "Playback #" ) 
( Indicator::LastPlayedDeltaNo, "Last delta #" ) 
( Indicator::LastPlayedKeyNo, "Last key #" ) 
( Indicator::PlayedNum, "Played frames" ) 
( Indicator::PlayedKeyNum, "Played key frames" ) 
( Indicator::SkippedNoKeyNum, "Skipped (no key)" ) 
( Indicator::SkippedIncompleteNum, "Skipped (incomplete)" ) 
( Indicator::SkippedBadGopNum, "Skipped (bad GOP)" ) 
( Indicator::SkippedIncompleteKeyNum, "Skipped (incomplete key)" ) 
( Indicator::LatencyEstimated, "Latency (est.)" ) 
( Indicator::SegmentsDeltaAvgNum, "Delta segments average" ) 
( Indicator::SegmentsKeyAvgNum, "Key segments average" ) 
( Indicator::SegmentsDeltaParityAvgNum, "Delta parity segments average" ) 
( Indicator::SegmentsKeyParityAvgNum, "Key parity segments average" ) 
( Indicator::RtxFrequency, "Retransmission frequency" ) 
( Indicator::RtxNum, "Retransmissions" ) 
( Indicator::RebufferingsNum, "Rebufferings" ) 
( Indicator::RequestedNum, "Requested" ) 
( Indicator::RequestedKeyNum, "Requested key" ) 
( Indicator::DW, "Default W" ) 
( Indicator::W, "Current W" ) 
( Indicator::RttPrime, "RTT'" ) 
( Indicator::InBitrateKbps, "Incoming bitrate (Kbps)" ) 
( Indicator::InRateSegments, "Incoming segments rate" ) 
( Indicator::SegmentsReceivedNum, "Segments received" ) 
( Indicator::TimeoutsNum, "Timeouts" ) 
( Indicator::RttEstimation, "RTT estimation" ) 
( Indicator::InterestRate, "Interest rate" ) 
( Indicator::QueueSize, "Interest queue" ) 
( Indicator::InterestsSentNum, "Sent interests" ) 
( Indicator::OutBitrateKbps, "Outgoing bitarte (Kbps)" ) 
( Indicator::OutRateSegments, "Outgoing segments rate" ) 
( Indicator::PublishedNum, "Published frames" ) 
( Indicator::PublishedKeyNum, "Published key frames" ) 
( Indicator::EncodingRate, "Encoding rate" ) 
( Indicator::CaptureRate, "Capture rate" ) 
( Indicator::CapturedNum, "Captured frames" );
/**
 * Unsupported initalizer list
 */
/*{
    { Indicator::AcquiredNum, "Acquired frames" },
    { Indicator::AcquiredKeyNum, "Acquired key frames" },
    { Indicator::DroppedNum, "Dropped frames" },
    { Indicator::DroppedKeyNum, "Dropped key frames" },
    { Indicator::AssembledNum, "Assembled frames" },
    { Indicator::AssembledKeyNum, "Assembled key frames" },
    { Indicator::RecoveredNum, "Recovered frames" },
    { Indicator::RecoveredKeyNum, "Recovered key frames" },
    { Indicator::RescuedNum, "Rescued frames" },
    { Indicator::RescuedKeyNum, "Rescued key frames" },
    { Indicator::IncompleteNum, "Incomplete frames" },
    { Indicator::IncompleteKeyNum, "Incomplete key frames" },
    { Indicator::BufferTargetSize, "Jitter target size" },
    { Indicator::BufferPlayableSize, "Jitter playable size" },
    { Indicator::BufferEstimatedSize, "Jitter estimated size" },
    { Indicator::CurrentProducerFramerate, "Producer rate" },
    
    { Indicator::LastPlayedNo, "Playback #" },
    { Indicator::LastPlayedDeltaNo, "Last delta #" },
    { Indicator::LastPlayedKeyNo, "Last key #" },
    { Indicator::PlayedNum, "Played frames" },
    { Indicator::PlayedKeyNum, "Played key frames" },
    { Indicator::SkippedNoKeyNum, "Skipped (no key)" },
    { Indicator::SkippedIncompleteNum, "Skipped (incomplete)" },
    { Indicator::SkippedBadGopNum, "Skipped (bad GOP)" },
    { Indicator::SkippedIncompleteKeyNum, "Skipped (incomplete key)" },
    { Indicator::LatencyEstimated, "Latency (est.)" },
    
    { Indicator::SegmentsDeltaAvgNum, "Delta segments average" },
    { Indicator::SegmentsKeyAvgNum, "Key segments average" },
    { Indicator::SegmentsDeltaParityAvgNum, "Delta parity segments average" },
    { Indicator::SegmentsKeyParityAvgNum, "Key parity segments average" },
    { Indicator::RtxFrequency, "Retransmission frequency" },
    { Indicator::RtxNum, "Retransmissions" },
    { Indicator::RebufferingsNum, "Rebufferings" },
    { Indicator::RequestedNum, "Requested" },
    { Indicator::RequestedKeyNum, "Requested key" },
    { Indicator::DW, "Default W" },
    { Indicator::W, "Current W" },
    { Indicator::RttPrime, "RTT'" },
    { Indicator::InBitrateKbps, "Incoming bitrate (Kbps)" },
    { Indicator::InRateSegments, "Incoming segments rate" },
    { Indicator::SegmentsReceivedNum, "Segments received" },
    { Indicator::TimeoutsNum, "Timeouts" },
    
    { Indicator::RttEstimation, "RTT estimation" },
    
    { Indicator::InterestRate, "Interest rate" },
    { Indicator::QueueSize, "Interest queue" },
    { Indicator::InterestsSentNum, "Sent interests" },
    
    { Indicator::OutBitrateKbps, "Outgoing bitarte (Kbps)" },
    { Indicator::OutRateSegments, "Outgoing segments rate" },
    { Indicator::PublishedNum, "Published frames" },
    { Indicator::PublishedKeyNum, "Published key frames" },
    
    { Indicator::EncodingRate, "Encoding rate" },
    
    { Indicator::CaptureRate, "Capture rate" },
    { Indicator::CapturedNum, "Captured frames" }
};
*/

const StatisticsStorage::StatRepo StatisticsStorage::ConsumerStatRepo =
map_list_of ( Indicator::AcquiredNum, 0. )
( Indicator::AcquiredKeyNum, 0. )
( Indicator::DroppedNum, 0 )
( Indicator::DroppedKeyNum, 0. )
( Indicator::AssembledNum, 0 )
( Indicator::AssembledKeyNum, 0. )
( Indicator::RecoveredNum, 0. )
( Indicator::RecoveredKeyNum, 0. )
( Indicator::RescuedNum, 0. )
( Indicator::RescuedKeyNum, 0. )
( Indicator::IncompleteNum, 0. )
( Indicator::IncompleteKeyNum, 0. )
( Indicator::BufferTargetSize, 0. )
( Indicator::BufferPlayableSize, 0. )
( Indicator::BufferEstimatedSize, 0. )
( Indicator::CurrentProducerFramerate, 0. )
( Indicator::LastPlayedNo, 0. )
( Indicator::LastPlayedDeltaNo, 0. )
( Indicator::LastPlayedKeyNo, 0. )
( Indicator::PlayedNum, 0. )
( Indicator::PlayedKeyNum, 0. )
( Indicator::SkippedNoKeyNum, 0. )
( Indicator::SkippedIncompleteNum, 0. )
( Indicator::SkippedBadGopNum, 0. )
( Indicator::SkippedIncompleteKeyNum, 0. )
( Indicator::LatencyEstimated, 0. )
( Indicator::SegmentsDeltaAvgNum, 0. )
( Indicator::SegmentsKeyAvgNum, 0. )
( Indicator::SegmentsDeltaParityAvgNum, 0. )
( Indicator::SegmentsKeyParityAvgNum, 0. )
( Indicator::RtxFrequency, 0. )
( Indicator::RtxNum, 0. )
( Indicator::RebufferingsNum, 0. )
( Indicator::RequestedNum, 0. )
( Indicator::RequestedKeyNum, 0. )
( Indicator::DW, 0. )
( Indicator::W, 0. )
( Indicator::RttPrime, 0. )
( Indicator::InBitrateKbps, 0. )
( Indicator::InRateSegments, 0. )
( Indicator::SegmentsReceivedNum, 0. )
( Indicator::TimeoutsNum, 0. )
( Indicator::RttEstimation, 0. )
( Indicator::InterestRate, 0. )
( Indicator::QueueSize, 0. )
( Indicator::InterestsSentNum, 0. );
/**
 * Unsupported initalizer list
 */
/*{
    { Indicator::AcquiredNum, 0. },
    { Indicator::AcquiredKeyNum, 0. },
    { Indicator::DroppedNum, 0 },
    { Indicator::DroppedKeyNum, 0. },
    { Indicator::AssembledNum, 0 },
    { Indicator::AssembledKeyNum, 0. },
    { Indicator::RecoveredNum, 0. },
    { Indicator::RecoveredKeyNum, 0. },
    { Indicator::RescuedNum, 0. },
    { Indicator::RescuedKeyNum, 0. },
    { Indicator::IncompleteNum, 0. },
    { Indicator::IncompleteKeyNum, 0. },
    { Indicator::BufferTargetSize, 0. },
    { Indicator::BufferPlayableSize, 0. },
    { Indicator::BufferEstimatedSize, 0. },
    { Indicator::CurrentProducerFramerate, 0. },
    
    { Indicator::LastPlayedNo, 0. },
    { Indicator::LastPlayedDeltaNo, 0. },
    { Indicator::LastPlayedKeyNo, 0. },
    { Indicator::PlayedNum, 0. },
    { Indicator::PlayedKeyNum, 0. },
    { Indicator::SkippedNoKeyNum, 0. },
    { Indicator::SkippedIncompleteNum, 0. },
    { Indicator::SkippedBadGopNum, 0. },
    { Indicator::SkippedIncompleteKeyNum, 0. },
    { Indicator::LatencyEstimated, 0. },
    
    { Indicator::SegmentsDeltaAvgNum, 0. },
    { Indicator::SegmentsKeyAvgNum, 0. },
    { Indicator::SegmentsDeltaParityAvgNum, 0. },
    { Indicator::SegmentsKeyParityAvgNum, 0. },
    { Indicator::RtxFrequency, 0. },
    { Indicator::RtxNum, 0. },
    { Indicator::RebufferingsNum, 0. },
    { Indicator::RequestedNum, 0. },
    { Indicator::RequestedKeyNum, 0. },
    { Indicator::DW, 0. },
    { Indicator::W, 0. },
    { Indicator::RttPrime, 0. },
    { Indicator::InBitrateKbps, 0. },
    { Indicator::InRateSegments, 0. },
    { Indicator::SegmentsReceivedNum, 0. },
    { Indicator::TimeoutsNum, 0. },
    
    { Indicator::RttEstimation, 0. },
    
    { Indicator::InterestRate, 0. },
    { Indicator::QueueSize, 0. },
    { Indicator::InterestsSentNum, 0. }
};
*/

const StatisticsStorage::StatRepo StatisticsStorage::ProducerStatRepo =
map_list_of  ( Indicator::OutBitrateKbps, 0. )
( Indicator::OutRateSegments, 0. )
( Indicator::PublishedNum, 0. )
( Indicator::PublishedKeyNum, 0. )
( Indicator::InterestRate, 0. )
( Indicator::DroppedNum, 0. )
( Indicator::EncodingRate, 0. )
( Indicator::CaptureRate, 0. )
( Indicator::CapturedNum, 0. );
/**
 * Unsupported initalizer list
 */
/*{
    { Indicator::OutBitrateKbps, 0. },
    { Indicator::OutRateSegments, 0. },
    { Indicator::PublishedNum, 0. },
    { Indicator::PublishedKeyNum, 0. },
    { Indicator::InterestRate, 0. },
    
    { Indicator::DroppedNum, 0. },
    { Indicator::EncodingRate, 0. },
    
    { Indicator::CaptureRate, 0. },
    { Indicator::CapturedNum, 0. }
};
*/

StatisticsStorage::StatRepo
StatisticsStorage::getIndicators() const
{
    StatRepo copy;
    for (StatRepo::const_iterator it = indicators_.begin(); it != indicators_.end(); ++it)
        copy[it->first] = it->second;
    return copy;
}

void
StatisticsStorage::updateIndicator(const statistics::Indicator& indicator,
                                   const double& value) throw(std::out_of_range)
{
    indicators_.at(indicator) = value;
}