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

        // playback latency (reliable only if consumer and producer are
        // ntp-synchronized)
        double latency_;
        double rttEstimation_;
        
        // buffers
        unsigned int rebufferingEvents_;
        unsigned int jitterPlayableMs_, jitterEstimationMs_, jitterTargetMs_;
        double actualProducerRate_;
        
        // frames
        unsigned int nPlayed_, nSkipped_, nWrongOrder_, /*nLost_,*/ nReceived_, nRescued_, nIncompleteTotal_, nIncompleteKey_, nRecovered_;
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
