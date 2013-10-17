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
    
    class ReceiverChannelParams : public NdnParams {
    public:
        
        static ReceiverChannelParams* defaultParams()
        {
            ReceiverChannelParams *p = new ReceiverChannelParams();
            
            char *host =  (char*)"localhost";
            int portnum = 6363;
            
            p->setStringParam(ParamNameConnectHost, host);
            p->setIntParam(ParamNameConnectPort, portnum);
            
            // add params for internal classes
            shared_ptr<VideoReceiverParams> vr_sp(VideoReceiverParams::defaultParams());
            shared_ptr<NdnRendererParams> rr_sp(NdnRendererParams::defaultParams());
            shared_ptr<NdnVideoCoderParams> vc_sp(NdnVideoCoderParams::defaultParams());
            
            p->addParams(*vr_sp.get());
            p->addParams(*rr_sp.get());
            p->addParams(*vc_sp.get());
            
            return p;
        }
        
        int getConnectPort() { return getParamAsInt(ParamNameConnectPort); }
        std::string getConnectHost() { return getParamAsString(ParamNameConnectHost); }
    };
    
    class NdnReceiverChannel : public NdnRtcObject
    {
    public:
        NdnReceiverChannel(NdnParams *params);
        virtual ~NdnReceiverChannel() { TRACE(""); }
        
        int init();
        int startFetching();
        int stopFetching();
        
        void getStat(ReceiverChannelStatistics &stat) const;
        
        // ndnlib callbacks
        void onInterest(const shared_ptr<const Name>& prefix, const shared_ptr<const Interest>& interest, ndn::Transport& transport);
        void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix);

//    private:
    protected:
        bool isTransmitting_;
        shared_ptr<ndn::Transport> ndnTransport_;
        shared_ptr<Face> ndnFace_;
        shared_ptr<NdnRenderer> localRender_;
        shared_ptr<NdnVideoDecoder> decoder_;
        shared_ptr<NdnVideoReceiver> receiver_;
        
        ReceiverChannelParams *getParams() { return static_cast<ReceiverChannelParams*>(params_); }
    };
}

#endif /* defined(__ndnrtc__receiver_channel__) */
