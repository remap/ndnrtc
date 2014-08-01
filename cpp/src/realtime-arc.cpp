//
//  realtime-arc.cpp
//  ndnrtc
//
//  Copyright 2014 Panasonic Coporation and University of California
//  For licensing details see the LICENSE file.
//
//  Authors:  Takahiro Yoneda, Peter Gusev
//

#ifdef USE_ARC

#include <limits>
#include <cmath>
#include <cstring>

#include "realtime-arc.h"

using namespace ndnrtc;

RealTimeAdaptiveRateControl::RealTimeAdaptiveRateControl ()
{
    indexSeq_ = lastEstSeq_ = lastRcvSeq_ = std::numeric_limits<uint32_t>::max ();
    avgDataSize_ = 0;
    minRttMs_ =  minRttMsNext_ = 0;
    congestionSign_ = false;
    interestPps_ = 0;
    prevAvgRttMs_ = 0;
    offsetJitterMs_ = 10;
}

RealTimeAdaptiveRateControl::~RealTimeAdaptiveRateControl ()
{
}


int
RealTimeAdaptiveRateControl::initialize(const CodecMode& codecMode,
                                        unsigned int nStreams,
                                        StreamEntry* streamArray)
{
    if (codecMode != CodecModeNormal) return -1;
    if (nStreams < 1 || nStreams > SSTBL_MAX) return -1;
    if (streamArray == NULL) return -1;
    codecMode_ = codecMode;
    numStream_ = nStreams;
    memcpy(switchStreamTable, streamArray, sizeof(StreamEntry) * nStreams);
    
    return 0;
}


void
RealTimeAdaptiveRateControl::interestExpressed(const std::string &name,
                                               unsigned int streamId)
{
    currentStreamId_ = streamId;
    ++indexSeq_;
    /*
    // insert entry of Interest history
    InterestHistries_.insert (InterestHistry (indexSeq_, name, streamId));
    */
    return;
}


void
RealTimeAdaptiveRateControl::interestTimeout(const std::string &name,
                                             unsigned int streamId)
{
	/*
    name_map& nmap = InterestHistries_.get<i_name> ();
    name_map::iterator entry = nmap.find(name);
    if (entry == nmap.end ()) return;
    // update entry of Interest history
    InterestHistry ih = *entry;
    ++ih.timeout_count;
    nmap.replace(entry, ih);
    */
    return;
}


void
RealTimeAdaptiveRateControl::dataReceived(const std::string &name,
                                          unsigned int streamId,
                                          unsigned int dataSize,
                                          double rttMs,
                                          unsigned int nRtx)
{
    //uint32_t seq, diff_seq;
    if (dataSize == 0 || rttMs < 0 || nRtx < 0) return;

    if (avgDataSize_ == 0)
        avgDataSize_ = dataSize;
    avgDataSize_ = (0.9 * avgDataSize_) + (0.1 * dataSize);

    /*
    name_map& nmap = InterestHistries_.get<i_name> ();
    name_map::iterator entry = nmap.find(name);
    
    if (avgDataSize_ == 0)
        avgDataSize_ = dataSize;
    avgDataSize_ = (0.9 * avgDataSize_) + (0.1 * dataSize);
    
    if (entry == nmap.end ()) return;
    
    seq = entry->GetSeq ();
    
    diff_seq = diffSeq (seq, lastRcvSeq_);
    if (diff_seq > 0)
        lastRcvSeq_ = seq;
    
    // update entry of Interest history
    InterestHistry ih = *entry;
    ++ih.rx_count;
    if (ih.rx_count == 1) {
        ih.rtt = rttMs;
        ih.retx_count = nRtx;
    }
    nmap.replace(entry, ih);
    */
    return;
}

void
RealTimeAdaptiveRateControl::getInterestRate(int64_t timestampMs,
                                             double& interestRate,
                                             unsigned int& streamId)
{
    unsigned int sid;
    double bitrate, parity_ratio;
    
    if (interestPps_ == 0)
        interestPps_ = convertBpsToPps (getStreamBitrate (currentStreamId_) * (1.0 + getStreamParityRatio (currentStreamId_)));

	if ((indexSeq_ % 10000) != 0)
		interestPps_ += (20 / sqrt (interestPps_));
	else
		interestPps_ -= 0.5 * interestPps_;
		
	getStreamByPps (interestPps_, sid, bitrate, parity_ratio);
    // check minimum interestRate (but no maximum inretestRate)
    if (convertBpsToPps ((bitrate * (1.0 + parity_ratio))) > interestPps_)
        interestRate = (X_BYTE / avgDataSize_) * convertBpsToPps ((bitrate * (1.0 + parity_ratio)));
    else
        interestRate = (X_BYTE / avgDataSize_) * interestPps_;
    streamId = sid;
    return;

	/*
    uint32_t start_seq = lastEstSeq_ + 1;
    unsigned int rx_count = 0;
    unsigned int loss_count = 0;
    int64_t sum_rtt = 0;
    int64_t avg_rtt = 0;
    unsigned int sid;
    double bitrate, parity_ratio;
    
    seq_map& smap = InterestHistries_.get<i_seq> ();
    
    if (timestampMs < 0) {
        interestRate = convertBpsToPps (getStreamBitrate (currentStreamId_) * (1.0 + getStreamParityRatio (currentStreamId_)));
        streamId = currentStreamId_;
        return;
    };
    
    // init interestPps_ for first api call
    if (interestPps_ == 0)
        interestPps_ = convertBpsToPps (getStreamBitrate (currentStreamId_) * (1.0 + getStreamParityRatio (currentStreamId_)));
    
    for(uint32_t i = start_seq; i <= indexSeq_; ++i) {
        seq_map::iterator tmp_entry = smap.find(i);
        if (tmp_entry != smap.end ()) {
            if (tmp_entry->GetRxCount () > 0
                && tmp_entry->GetRetxCount () == 0
                && tmp_entry->GetTimeoutCount () == 0) {
                sum_rtt += tmp_entry->GetRtt ();
                ++rx_count;
                lastEstSeq_ = i;
                
                // update minimum RTT
                if (minRttMs_ == 0) {
                    minRttMs_ = tmp_entry->GetRtt ();
                    updateMinRttTs_ = timestampMs;
                }
                if (minRttMsNext_ == 0 || minRttMsNext_ > tmp_entry->GetRtt ()) {
                    minRttMsNext_ = tmp_entry->GetRtt ();
                }
                if (diffTimestamp (timestampMs, updateMinRttTs_) > MIN_RTT_EXPIRE_MS) {
                    minRttMs_ = minRttMsNext_;
                    minRttMsNext_ = 0;
                    updateMinRttTs_ = timestampMs;
                }
            }
            else if (tmp_entry->GetTimeoutCount () > 0) {
                ++loss_count;
                lastEstSeq_ = i;
            }
            else {
                break;
            }
            //delete entry of Interest history
            smap.erase (i);
        }
    }
    
    if (rx_count > 0) {
        avg_rtt = sum_rtt / rx_count;
        if (prevAvgRttMs_ == 0)
            prevAvgRttMs_ = avg_rtt;
        
        if (avg_rtt <= (minRttMs_ + offsetJitterMs_) && loss_count == 0 && !congestionSign_) {
            interestPps_ += (20 / sqrt (interestPps_));
        }
        else if (avg_rtt <= prevAvgRttMs_ && loss_count == 0) {
            if (!congestionSign_) {
                interestPps_ += (20 / sqrt (interestPps_));
            }
            else {
                congestionSign_ = false;
            }
        }
        else if (avg_rtt > prevAvgRttMs_ && loss_count == 0) {
            if (congestionSign_) {
                interestPps_ -= (0.5 * sqrt (interestPps_));
            }
            else {
                congestionSign_ = true;
            }
        }
        // if rx_count > 0 && loss_count > 0
        else {
            interestPps_ -= (0.5 * sqrt (interestPps_));
            congestionSign_ = true;
        }
        prevAvgRttMs_ = avg_rtt;
    }
    else if (loss_count > 0) {
        interestPps_ -= (0.5 * sqrt (interestPps_));
        congestionSign_ = true;
    }
    
    getStreamByPps (interestPps_, sid, bitrate, parity_ratio);
    // check minimum interestRate (but no maximum inretestRate)
    if (convertBpsToPps ((bitrate * (1.0 + parity_ratio))) > interestPps_)
        interestRate = (X_BYTE / avgDataSize_) * convertBpsToPps ((bitrate * (1.0 + parity_ratio)));
    else
        interestRate = (X_BYTE / avgDataSize_) * interestPps_;
    streamId = sid;
    return;
    */
}


void
RealTimeAdaptiveRateControl::getStreamByPps (double pps,
                                             unsigned int& id,
                                             double& bitrate,
                                             double& parity_ratio)
{
    StreamEntry *entry = switchStreamTable;
    double bps = convertPpsToBps (pps);
    double select_bps = 0;
    double select_ratio = 0;
    unsigned int select_id;
    double min_bps = 0;
    double min_ratio = 0;
    double min_id;
    
    for (unsigned int i = 0; i < numStream_; ++i) {
        //search for sutable stream
        if ((entry->bitrate_ * (1.0 + entry->parityRatio_)) <= bps
            && ((entry->bitrate_ > select_bps) || (entry->bitrate_ == select_bps && entry->parityRatio_ > select_ratio))) {
            select_bps = entry->bitrate_;
            select_ratio = entry->parityRatio_;
            select_id = entry->id_;
        }
        // serch for minimul stream
        if (entry->bitrate_ < min_bps
            || (entry->bitrate_ == min_bps && entry->parityRatio_ < min_ratio)
            || (min_bps == 0 && min_ratio == 0)) {
            min_bps = entry->bitrate_;
            min_ratio = entry->parityRatio_;
            min_id = entry->id_;
        }
        
        ++entry;
    }
    
    if (select_bps == 0) {
        id =  min_id;
        bitrate = min_bps;
        parity_ratio = min_ratio;
    }
    else {
        id = select_id;
        bitrate = select_bps;
        parity_ratio = select_ratio;
    }
}

double
RealTimeAdaptiveRateControl::getStreamBitrate (unsigned int id)
{
    StreamEntry *entry = switchStreamTable;
    for (unsigned int i = 0; i < numStream_; ++i) {
        if (entry->id_ == id)
            return entry->bitrate_;
        ++entry;
    }
    return 0;
}

double
RealTimeAdaptiveRateControl::getStreamParityRatio (unsigned int id)
{
    StreamEntry *entry = switchStreamTable;
    for (unsigned int i = 0; i < numStream_; ++i) {
        if (entry->id_ == id)
            return entry->parityRatio_;
        ++entry;
    }
    return 0;
}

double
RealTimeAdaptiveRateControl::convertPpsToBps (double pps)
{
    return (8 * X_BYTE * pps);
}

double
RealTimeAdaptiveRateControl::convertBpsToPps (double bps)
{
    return (bps / (8 * X_BYTE));
}

uint32_t
RealTimeAdaptiveRateControl::diffSeq (uint32_t a, uint32_t b)
{
    uint32_t diff;
    if (a >= b) {
        diff = a - b;
    }
    else {
        if ((b - a) > (std::numeric_limits<uint32_t>::max () / 2))
            diff = a + (std::numeric_limits<uint32_t>::max () - b);
        else
            diff = a - b;
    }
    return diff;
}

int64_t
RealTimeAdaptiveRateControl::diffTimestamp (int64_t a, int64_t b)
{
    int64_t diff;
    if (a < 0 || b < 0) std::abort ();
    if (a >= b) {
        diff = a - b;
    }
    else {
        if ((b - a) > (std::numeric_limits<int64_t>::max () / 2))
            diff = a + (std::numeric_limits<int64_t>::max () - b);
        else
            diff = a - b;
    }
    return diff;
}


int64_t
RealTimeAdaptiveRateControl::addTimestamp (int64_t a, int64_t b)
{
    int64_t diff_a, diff_b;
    if (a < 0 || b < 0) std::abort ();
    
    diff_a = std::numeric_limits<int64_t>::max () - a;
    diff_b = std::numeric_limits<int64_t>::max () - b;
    
    if (diff_a < b)
        return (b - diff_a - 1);
    if (diff_b < a)
        return (a - diff_b - 1);
    else
        return (a + b);
}

#endif
