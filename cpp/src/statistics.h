//
//  statistics.h
//  ndnrtc
//
//  Created by Peter Gusev on 11/15/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef ndnrtc_statistics_h
#define ndnrtc_statistics_h

namespace ndnrtc {
    
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

        // RTT values for packets
        double srtt_;
        // playback latency (reliable only if consumer and producer are
        // ntp-synchronized)
        double latency_;
        double rttEstimation_;
        
        // buffers
        unsigned int rebufferingEvents_;
        unsigned int jitterPlayableMs_, jitterEstimationMs_, jitterTargetMs_;
        double actualProducerRate_;
        
        // buffer stat
        unsigned int nSent_, nAssembling_;
        
        // frames
        unsigned int nPlayed_, nMissed_, nLost_, nReceived_, nRescued_, nIncomplete_, nRecovered_;
        
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
#if 0
        // recent frame numbers:
        unsigned int nPlayback_, nPipeline_, nFetched_, nLate_;
        
        // errors - number of total skipped frames and timeouts
        unsigned int nTimeouts_, nTotalTimeouts_, nSkipped_;
        
        // frame buffer info
        unsigned int nFree_, nLocked_, nAssembling_, nNew_;
        
        // produce statistics
        unsigned int sentNo_;
        double sendingFramesFreq_, capturingFreq_; // latest sent frame number
        
        double inFramesFreq_, inDataFreq_, playoutFreq_;
#endif
        SenderChannelStatistics sendStat_;
        ReceiverChannelStatistics receiveStat_;
        
    } NdnLibStatistics;
    
}

#endif
