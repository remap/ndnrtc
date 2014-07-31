//
//  statistics.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef ndnrtc_statistics_h
#define ndnrtc_statistics_h

namespace ndnrtc {
    
    // jitter buffer statistics
    typedef struct _BufferStatistics {
        unsigned int
        nAcquired_ = 0,     // number of frames acquired for playback
        nAcquiredKey_ = 0,  // number of key frames acquired for playback
        nDropped_ = 0,      // number of outdated frames dropped
        nDroppedKey_ = 0;   // number of outdated key frames dropped
        
        unsigned int
        nAssembled_ = 0,    // number of frames fully assembled
        nAssembledKey_ = 0, // number of key frames fully assembled
        nRescued_ = 0,      // number of frames assembled using interest
                            // retransmissions
        nRescuedKey_ = 0,   // number of key frames assembled using interest
                            // retransmissions
        nRecovered_ = 0,    // number of frames recovered using FEC
        nRecoveredKey_ = 0, // number of key frames recovered using FEC
        nIncomplete_ = 0,   // number of incomplete frames (frames missing some
                            // segments by the time they should be played out)
        nIncompleteKey_ = 0;    // number of incomplete key frames
    } BufferStatistics;
    
    // playout statistics
    typedef struct _PlayoutStatistics {
        unsigned int
        nPlayed_ = 0,       // number of frames actually played (rendered)
        nPlayedKey_ = 0,    // number of key frames played
        nSkippedNoKey_ = 0, // number of delta frames skipped due to wrong GOP
                            // (i.e. corresponding key frame has not been used
                            // yet or has been already used)
        nSkippedIncomplete_ = 0,    // number of frames skipped due to
                                    // frame incompleteness
        nSkippedInvalidGop_ = 0,    // number of frames skipped due to previously
                                    // broken GOP sequence
        nSkippedIncompleteKey_ = 0; // number of key frames skipped due to frame
                                    // incompleteness
        double latency_ = 0.;   // if consumer and producer are synchronized to
                                // the same ntp server, this value could make
                                // sense for debugging
    } PlayoutStatistics;
    
    // sending channel statistics
    typedef struct _SenderChannelPerformance {
        double nBytesPerSec_;
        double nFramesPerSec_, encodingRate_;
        unsigned int lastFrameNo_, nDroppedByEncoder_;
    } SenderChannelPerformance;
    
    typedef struct SenderChannelStatistics {
        SenderChannelPerformance videoStat_, audioStat_;
    } SenderChannelStatistics;
    
    // receiving channel statistics
    typedef struct _ReceiverChannelPerformance {
        double nBytesPerSec_, interestFrequency_, segmentsFrequency_;
        double rttEstimation_;
        
        // buffers
        unsigned int rebufferingEvents_;
        unsigned int jitterPlayableMs_, jitterEstimationMs_, jitterTargetMs_;
        double actualProducerRate_;
        
        PlayoutStatistics playoutStat_;
        BufferStatistics bufferStat_;
        
        unsigned int nInterestSent_, nDataReceived_, nTimeouts_;
        
        double segNumDelta_, segNumKey_;
        
        unsigned int rtxNum_;
        double rtxFreq_;
        
    } ReceiverChannelPerformance;
    
    typedef struct _ReceiverChannelStatistics {
        ReceiverChannelPerformance videoStat_, audioStat_;
    } ReceiverChannelStatistics;
    
    // library statistics
    typedef struct _NdnLibStatistics {
        // consume statistics:
        // current producer index (as we fetch video seamlessly)
        const char *producerId_;

        SenderChannelStatistics sendStat_;
        ReceiverChannelStatistics receiveStat_;
        
    } NdnLibStatistics;
    
}

#endif
