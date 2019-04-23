//
//  statistics.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author: Peter Gusev
//

#include "statistics.hpp"

#include <algorithm>
#include <boost/assign.hpp>

using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace boost::assign;

const std::map<Indicator, std::string> StatisticsStorage::IndicatorNames =
map_list_of
( Indicator::Timestamp, "Timestamp" )
// consumer
( Indicator::BufferedSlotsNum, "Buffered slots" )
( Indicator::SlotsReadyNum, "Slots ready" )
( Indicator::SlotsUnfetchableNum, "Slots unfetchable" )
( Indicator::BufferDelayEstimate, "Buffer delay estimate" )
( Indicator::FrameReassemblyEstimate, "Frame re-assembly delay estimate" )
( Indicator::DoubleRttNum, "Frames required additional round-trip")

// buffer
( Indicator::AcquiredNum, "Acquired frames" )
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
( Indicator::BufferReservedSize, "Jitter reserved size" )
( Indicator::CurrentProducerFramerate, "Producer rate" )
( Indicator::VerifySuccess, "Verified samples" )
( Indicator::VerifyFailure, "Verify failure samples" )
( Indicator::LatencyControlStable, "Latency control stable state" )
( Indicator::LatencyControlCommand, "Latency control command" )
( Indicator::FrameFetchAvgDelta, "Average time for fetching delta frames" )
( Indicator::FrameFetchAvgKey, "Average time for fetching key frames" )

// playout
( Indicator::LastPlayedNo, "Playback #" )
( Indicator::LastPlayedDeltaNo, "Last delta #" ) 
( Indicator::LastPlayedKeyNo, "Last key #" ) 
( Indicator::PlayedNum, "Played frames" ) 
( Indicator::PlayedKeyNum, "Played key frames" ) 
( Indicator::SkippedNum, "Skipped" )
( Indicator::LatencyEstimated, "Latency (est.)" )
// pipeliner
( Indicator::SegmentsDeltaAvgNum, "Delta segments average" ) 
( Indicator::SegmentsKeyAvgNum, "Key segments average" ) 
( Indicator::SegmentsDeltaParityAvgNum, "Delta parity segments average" ) 
( Indicator::SegmentsKeyParityAvgNum, "Key parity segments average" )
( Indicator::RtxNum, "Retransmissions" )
( Indicator::RebufferingsNum, "Rebufferings" ) 
( Indicator::RequestedNum, "Requested" ) 
( Indicator::RequestedKeyNum, "Requested key" ) 
( Indicator::DW, "Lambda D" )
( Indicator::W, "Lambda" )
( Indicator::SegmentsReceivedNum, "Segments received" )
( Indicator::TimeoutsNum, "Timeouts" )
( Indicator::NacksNum, "Nacks" )
( Indicator::AppNackNum, "App Nacks" )
( Indicator::Darr, "Darr" )
( Indicator::BytesReceived, "Payload bytes received" )
( Indicator::RawBytesReceived, "Wire bytes received" )
( Indicator::State, "Consumer state" )
( Indicator::DoubleRtFrames, "Number of frames with additional round trips for assembling" )
( Indicator::DoubleRtFramesKey, "Number of key frames with additional round trips for assemnbling" )
// DRD estimator
( Indicator::DrdOriginalEstimation, "DRD estimation (orig)" )
( Indicator::DrdCachedEstimation, "DRD estimation (cach)" )
// interest queue
( Indicator::QueueSize, "Interest queue" )
( Indicator::InterestsSentNum, "Sent interests" )
( Indicator::OutOfOrderNum, "Out of Order deliveries number" )

// producer
// media thread
( Indicator::BytesPublished, "Payload published bytes" )
( Indicator::FecBytesPublished, "FEC data published bytes" )
( Indicator::RawBytesPublished, "Wire published bytes" )
( Indicator::ProcessedNum, "Processed frames" )
( Indicator::PublishedSegmentsNum, "All published segments" )
( Indicator::FecPublishedSegmentsNum, "Published FEC segments" )
( Indicator::PublishedNum, "Published frames" ) 
( Indicator::PublishedKeyNum, "Published key frames" )
( Indicator::InterestsReceivedNum, "Interests received" )
( Indicator::SignNum, "Sign operations")
( Indicator::RdrPointerNum, "RDR pointer num")
( Indicator::PublishDelay, "Publish delay")

// estimators
(Indicator::FrameSizeEstimate, "Producer frame size estimation")
(Indicator::SegnumEstimate, "Producer frame segment number estimation")

// encoder
( Indicator::EncodedNum, "Encoded frames" )
( Indicator::GopNum, "GOP number" )
( Indicator::CodecDelay, "Coding delay")

// capturer
( Indicator::CapturedNum, "Captured frames" );

const StatisticsStorage::StatRepo StatisticsStorage::ConsumerStatRepo =
map_list_of
( Indicator::Timestamp, 0. )

// consumer
( Indicator::BufferedSlotsNum, 0. )
( Indicator::SlotsReadyNum, 0. )
( Indicator::SlotsUnfetchableNum, 0. )
( Indicator::BufferDelayEstimate, 0. )
( Indicator::FrameReassemblyEstimate, 0. )
( Indicator::DoubleRttNum, 0.)

// buffer
( Indicator::AcquiredNum, 0. )
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
( Indicator::BufferReservedSize, 0. )
( Indicator::CurrentProducerFramerate, 0. )
( Indicator::VerifySuccess, 0. )
( Indicator::VerifyFailure, 0. )
( Indicator::LatencyControlStable, 0. )
( Indicator::LatencyControlCommand, 0. )
( Indicator::FrameFetchAvgDelta, 0. )
( Indicator::FrameFetchAvgKey, 0. )
// playout
( Indicator::LastPlayedNo, 0. )
( Indicator::LastPlayedDeltaNo, 0. )
( Indicator::LastPlayedKeyNo, 0. )
( Indicator::PlayedNum, 0. )
( Indicator::PlayedKeyNum, 0. )
( Indicator::SkippedNum, 0. )
( Indicator::LatencyEstimated, 0. )
( Indicator::CodecDelay, 0.)

// pipeliner
( Indicator::SegmentsDeltaAvgNum, 0. )
( Indicator::SegmentsKeyAvgNum, 0. )
( Indicator::SegmentsDeltaParityAvgNum, 0. )
( Indicator::SegmentsKeyParityAvgNum, 0. )
( Indicator::RtxNum, 0. )
( Indicator::RebufferingsNum, 0. )
( Indicator::RequestedNum, 0. )
( Indicator::RequestedKeyNum, 0. )
( Indicator::DW, 0. )
( Indicator::W, 0. )
( Indicator::SegmentsReceivedNum, 0. )
( Indicator::TimeoutsNum, 0. )
( Indicator::NacksNum, 0. )
( Indicator::AppNackNum, 0. )
( Indicator::Darr, 0. )
( Indicator::BytesReceived, 0. )
( Indicator::RawBytesReceived, 0. )
( Indicator::State, 0. )
( Indicator::DoubleRtFrames, 0. )
( Indicator::DoubleRtFramesKey, 0. )
// DRD estimator
( Indicator::DrdCachedEstimation, 0. )
( Indicator::DrdOriginalEstimation, 0. )
// interest queue
( Indicator::QueueSize, 0. )
( Indicator::InterestsSentNum, 0. )
( Indicator::OutOfOrderNum, 0. );

const StatisticsStorage::StatRepo StatisticsStorage::ProducerStatRepo =
map_list_of ( Indicator::Timestamp, 0. )
// producer
// media thread
( Indicator::BytesPublished, 0. )
( Indicator::FecBytesPublished, 0. )
( Indicator::RawBytesPublished, 0. )
( Indicator::PublishedSegmentsNum, 0. )
( Indicator::FecPublishedSegmentsNum, 0. )
( Indicator::ProcessedNum, 0. )
( Indicator::PublishedNum, 0. )
( Indicator::PublishedKeyNum, 0. )
( Indicator::InterestsReceivedNum, 0. )
( Indicator::SignNum, 0. )
( Indicator::CurrentProducerFramerate, 0. )
( Indicator::RdrPointerNum, 0. )
( Indicator::PublishDelay, 0.)
// estimators
( Indicator::FrameSizeEstimate, 0. )
( Indicator::SegnumEstimate, 0. )
// encoder
( Indicator::DroppedNum, 0. )
( Indicator::EncodedNum, 0. )
( Indicator::GopNum, 0. )
( Indicator::CodecDelay, 0.)
// capturer
( Indicator::CapturedNum, 0. );

// all statistics indicator names
const std::map<Indicator, std::string> StatisticsStorage::IndicatorKeywords =
boost::assign::map_list_of
(Indicator::Timestamp, "timestamp")

// consumer
( Indicator::BufferedSlotsNum, "bufSlots" )
( Indicator::SlotsReadyNum, "slotsReady" )
( Indicator::SlotsUnfetchableNum, "slotsUnfetch" )
( Indicator::BufferDelayEstimate, "bufDelayEst" )
( Indicator::FrameReassemblyEstimate, "reasmEst" )
( Indicator::DoubleRttNum, "dblRttNum")

// buffer
(Indicator::AcquiredNum, "framesAcq")
(Indicator::AcquiredKeyNum, "framesAcqKey")
(Indicator::DroppedNum, "framesDrop")
(Indicator::DroppedKeyNum, "framesDropKey")
(Indicator::AssembledNum, "framesAsm")
(Indicator::AssembledKeyNum, "framesAsmKey")
(Indicator::RecoveredNum, "framesRec")
(Indicator::RecoveredKeyNum, "framesRecKey")
(Indicator::RescuedNum, "framesResc")
(Indicator::RescuedKeyNum, "framesRescKey")
(Indicator::IncompleteNum, "framesInc")
(Indicator::IncompleteKeyNum, "framesIncKey")
(Indicator::BufferTargetSize, "jitterTar")
(Indicator::BufferPlayableSize, "jitterPlay")
(Indicator::BufferReservedSize, "jitterRsrv")
(Indicator::CurrentProducerFramerate, "prodRate")
(Indicator::VerifySuccess, "verifySuccess")
(Indicator::VerifyFailure, "verifyFailure")
(Indicator::LatencyControlStable, "latCtrlStable" )
(Indicator::LatencyControlCommand, "latCtrlCmd" )
( Indicator::FrameFetchAvgDelta, "fetchDeltaAvg" )
( Indicator::FrameFetchAvgKey, "fetchKeyAvg" )
// playout
(Indicator::LastPlayedNo, "playNo")
(Indicator::LastPlayedDeltaNo, "deltaNo")
(Indicator::LastPlayedKeyNo, "keyNo")
(Indicator::PlayedNum, "framesPlayed")
(Indicator::PlayedKeyNum, "framesPlayedKey")
(Indicator::SkippedNum, "skipNoKey")
(Indicator::LatencyEstimated, "latEst")
// pipeliner
(Indicator::SegmentsDeltaAvgNum, "segAvgDelta")
(Indicator::SegmentsKeyAvgNum, "segAvgKey")
(Indicator::SegmentsDeltaParityAvgNum, "segAvgDeltaPar")
(Indicator::SegmentsKeyParityAvgNum, "segAvgKeyPar")
(Indicator::RtxNum, "rtxNum")
(Indicator::RebufferingsNum, "rebuf")
(Indicator::RequestedNum, "framesReq")
(Indicator::RequestedKeyNum, "framesReqKey")
(Indicator::DW, "lambdaD")
(Indicator::W, "lambda")
(Indicator::SegmentsReceivedNum, "segNumRcvd")
(Indicator::TimeoutsNum, "timeouts")
(Indicator::NacksNum, "nacks")
(Indicator::AppNackNum, "appNacks")
(Indicator::Darr, "dArr")
(Indicator::BytesReceived, "bytesRcvd")
(Indicator::RawBytesReceived, "rawBytesRcvd")
(Indicator::State, "state" )
( Indicator::DoubleRtFrames, "doubleRt" )
( Indicator::DoubleRtFramesKey, "doubleRtKey" )
// DRD estimator
(Indicator::DrdOriginalEstimation, "drdEst")
(Indicator::DrdCachedEstimation, "drdPrime")
// interest queue
(Indicator::QueueSize, "iqueue")
(Indicator::InterestsSentNum, "isent")
(Indicator::OutOfOrderNum, "outOfOrderNum")
// producer
(Indicator::BytesPublished, "bytesPub")
(Indicator::FecBytesPublished, "fecBytesPub")
(Indicator::RawBytesPublished, "rawBytesPub")
(Indicator::PublishedSegmentsNum, "segPub")
(Indicator::FecPublishedSegmentsNum, "fecSegPub")
(Indicator::ProcessedNum, "framesProcessed")
(Indicator::PublishedNum, "framesPub")
(Indicator::PublishedKeyNum, "framesPubKey")
(Indicator::InterestsReceivedNum, "irecvd")
(Indicator::SignNum, "signNum")
(Indicator::RdrPointerNum, "rdrNum")
(Indicator::PublishDelay, "pubDelay")
// estimators
(Indicator::FrameSizeEstimate, "frameSizeEst")
(Indicator::SegnumEstimate, "segNumEst")
// encoder
(Indicator::EncodedNum, "framesEncoded")
(Indicator::GopNum, "gopNum")
(Indicator::CodecDelay, "codecDelay")
// capturer
(Indicator::CapturedNum, "framesCaptured");

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
