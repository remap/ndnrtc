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
#if 0 // suspended
    class NdnReceiverChannel : public NdnMediaChannel
    {
    public:
        NdnReceiverChannel(const ParamsStruct &params,
                           const ParamsStruct &audioParams);
        virtual ~NdnReceiverChannel() { }
        
        int init();
        int startTransmission();
        int stopTransmission();

        void getChannelStatistics(ReceiverChannelStatistics &stat);
        void setLogger(NdnLogger *logger){
            LoggerObject::setLogger(logger);
            
            localRender_->setLogger(logger);
            decoder_->setLogger(logger);
            receiver_->setLogger(logger);
            audioReceiveChannel_->setLogger(logger);
        }
    protected:
        shared_ptr<NdnRenderer> localRender_;
        shared_ptr<NdnVideoDecoder> decoder_;
        shared_ptr<NdnVideoReceiver> receiver_;
        shared_ptr<NdnAudioReceiveChannel> audioReceiveChannel_;
    };
#endif
}

#endif /* defined(__ndnrtc__receiver_channel__) */
