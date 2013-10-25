//
//  media-receiver.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/22/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__media_receiver__
#define __ndnrtc__media_receiver__

#include "ndnrtc-common.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "media-sender.h"

namespace ndnrtc
{
    class NdnMediaReceiver : public NdnRtcObject {
    public:
        NdnMediaReceiver(const ParamsStruct &params);
        ~NdnMediaReceiver();
        
        virtual int init(shared_ptr<Face> face);
        virtual int startFetching();
        virtual int stopFetching();
        
    protected:
        enum ReceiverMode {
            ReceiverModeCreated,
            ReceiverModeInit,
            ReceiverModeStarted,
            ReceiverModeWaitingFirstSegment,
            ReceiverModeFetch,
            ReceiverModeChase
        };
        
        ReceiverMode mode_;

        long pipelinerFrameNo_;
        int pipelinerEventsMask_, interestTimeoutMs_;
        unsigned int producerSegmentSize_;
        Name framesPrefix_;
        shared_ptr<Face> face_;
        
        FrameBuffer frameBuffer_;
        PlayoutBuffer playoutBuffer_;
        
        webrtc::CriticalSectionWrapper &faceCs_;    // needed for synchrnous
                                                    // access to the NDN face
                                                    // object
        webrtc::ThreadWrapper &pipelineThread_, &assemblingThread_;
        
        // static routines for threads
        static bool pipelineThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processInterests();
        }
        static bool assemblingThreadRoutine(void *obj)
        {
            return ((NdnMediaReceiver*)obj)->processAssembling();
        }
        
        // thread main functions (called iteratively by static routines)
        virtual bool processInterests();
        virtual bool processAssembling();
        
        // ndn-lib callbacks
        virtual void onTimeout(const shared_ptr<const Interest>& interest);
        virtual void onSegmentData(const shared_ptr<const Interest>& interest,
                                   const shared_ptr<Data>& data);
        
        virtual void switchToMode(ReceiverMode mode);
        
        void requestInitialSegment();
        void pipelineInterests(FrameBuffer::Event &event);
        void requestSegment(unsigned int frameNo, unsigned int segmentNo);
        bool isStreamInterest(Name prefix);
        unsigned int getFrameNumber(Name prefix);
        unsigned int getSegmentNumber(Name prefix);
        void expressInterest(Name &prefix);
        void expressInterest(Interest &i);
        
        // derived classes should determine whether a frame with frameNo number
        // is late or not
        virtual bool isLate(unsigned int frameNo);
        
        unsigned int getTimeout() const;
        unsigned int getFrameBufferSize() const;
        unsigned int getFrameSlotSize() const;
        
    };
}

#endif /* defined(__ndnrtc__media_receiver__) */
