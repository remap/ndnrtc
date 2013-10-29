//
//  receiver-channel.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__receiver_channel__
#define __ndnrtc__receiver_channel__

#include "ndnrtc-common.h"
#include "video-receiver.h"
#include "video-decoder.h"
#include "renderer.h"
#include "sender-channel.h"
#include "audio-channel.h"

namespace ndnrtc
{
    typedef struct _ReceiverChannelStatistics {
        // recent frame numbers:
        unsigned int nPlayback_, nPipeline_, nFetched_, nLate_;
        
        // errors - number of total skipped frames and timeouts
        unsigned int nTimeouts_, nTotalTimeouts_, nSkipped_;
        
        // frame buffer info
        unsigned int nFree_, nLocked_, nAssembling_, nNew_;
        
        //
        double playoutFreq_, inDataFreq_, inFramesFreq_;
    } ReceiverChannelStatistics;
    
    class NdnReceiverChannel : public NdnMediaChannel
    {
    public:
        NdnReceiverChannel(const ParamsStruct &params,
                           const ParamsStruct &audioParams);
        virtual ~NdnReceiverChannel() { }
        
        int init();
        int startTransmission();
        int stopTransmission();
        
        void getStat(ReceiverChannelStatistics &stat) const;

    protected:
        shared_ptr<NdnRenderer> localRender_;
        shared_ptr<NdnVideoDecoder> decoder_;
        shared_ptr<NdnVideoReceiver> receiver_;
        shared_ptr<NdnAudioReceiveChannel> audioReceiveChannel_;
    };
}

#endif /* defined(__ndnrtc__receiver_channel__) */
