//
//  video-receiver.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef __ndnrtc__video_receiver__
#define __ndnrtc__video_receiver__

#include "ndnrtc-common.h"
#include "video-sender.h"
#include "frame-buffer.h"
#include "playout-buffer.h"

namespace ndnrtc
{
    class VideoReceiverParams : public VideoSenderParams {
    public:
        // static
        static VideoReceiverParams* defaultParams()
        {
            VideoReceiverParams *p = static_cast<VideoReceiverParams*>(VideoSenderParams::defaultParams());
            
            p->setIntParam(ParamNameProducerRate, 30);
            
            return p;
        }
        
        // parameters names
        static const std::string ParamNameProducerRate;
        static const std::string ParamNameReceiverId;
        
        // public methods go here
        int getProducerRate() const { return getParamAsInt(ParamNameProducerRate); }
        int getReceiverId() const { return getParamAsInt(ParamNameReceiverId); }
    };
    
    // used by decoder to notify frame provider that frame decodin has finished and
    // it can release frame buffer, cleanup, etc.
//    class IEncodedFrameProvider {
//    public:
//        void onFrameProcessed(Encoded) = 0;
//    }
    
    class NdnVideoReceiver : public NdnRtcObject {
    public:
        NdnVideoReceiver(NdnParams *params);
        ~NdnVideoReceiver();
        
        int init(shared_ptr<Face> face);
        int startFetching();
        int stopFetching();
        
    private:
        enum ReceiverMode {
            ReceiverModeCreated,
            ReceiverModeInit,
            ReceiverModeWaitingFirstSegment,
            ReceiverModeFetch,
            ReceiverModeChase
        };
        
        ReceiverMode mode_;
        
        bool playout_;
        long playoutSleepIntervalUSec_; // 30 fps
        long playoutFrameNo_;
        
        shared_ptr<Face> face_;
        FrameBuffer frameBuffer_;
        PlayoutBuffer playoutBuffer_;
        IEncodedFrameConsumer *frameConsumer_;
        
        webrtc::ThreadWrapper &playoutThread_, &pipelineThread_, &assemblingThread_;
        
        // static routines for threads to
        static bool playoutThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processPlayout(); }
        static bool pipelineThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processInterests(); }
        static bool assemblingThreadRoutine(void *obj) { return ((NdnVideoReceiver*)obj)->processAssembling(); }
        
        // thread main functions (called iteratively)
        bool processPlayout();
        bool processInterests();
        bool processAssembling();
        
        // ndn-lib callbacks
        void onTimeout(const shared_ptr<const Interest>& interest);
        void onSegmentData(const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data);
        
        VideoReceiverParams *getParams() { return static_cast<VideoReceiverParams*>(params_); }
        
    };
}

#endif /* defined(__ndnrtc__video_receiver__) */
