//
//  video-playout.h
//  ndnrtc
//
//  Created by Peter Gusev on 3/19/14.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __ndnrtc__video_playout__
#define __ndnrtc__video_playout__

#include "ndnrtc-common.h"
#include "playout.h"
#include "frame-buffer.h"

namespace ndnrtc {
    template<typename T>
    class VideoFramePacketT;
    struct Immutable;
    typedef VideoFramePacketT<Immutable> ImmutableFrameAlias;
    class IEncodedFrameConsumer;
    class IPlaybackQueue;
    class IVideoPlayoutObserver;
    
    class VideoPlayout : public Playout
    {
    public:
        VideoPlayout(boost::asio::io_service& io,
            const boost::shared_ptr<IPlaybackQueue>& queue,
            const boost::shared_ptr<StatStorage>& statStorage = 
                boost::shared_ptr<StatStorage>(StatStorage::createConsumerStatistics()));
        ~VideoPlayout(){}
        
        void stop();
        void registerFrameConsumer(IEncodedFrameConsumer* frameConsumer);
        void deregisterFrameConsumer();

        void attach(IVideoPlayoutObserver* observer);
        void detach(IVideoPlayoutObserver* observer);
        
    private:
        using Playout::attach;
        using Playout::detach;

        VideoFrameSlot frameSlot_;
        IEncodedFrameConsumer *frameConsumer_;
        bool gopIsValid_;
        PacketNumber currentPlayNo_;
        int gopCount_;

        void
        processSample(const boost::shared_ptr<const BufferSlot>&);
    };

    class IEncodedFrameConsumer 
    {
    public:
        virtual void processFrame(const boost::shared_ptr<ImmutableFrameAlias>&) = 0;
    };

    class IVideoPlayoutObserver : public IPlayoutObserver 
    {
    public:
        virtual void frameSkipped(PacketNumber pNo, bool isKey) = 0;
        virtual void frameProcessed(PacketNumber pNo, bool isKey) = 0;
        virtual void recoveryFailure(PacketNumber sampleNo, bool isKey) = 0;
    };
}

#endif /* defined(__ndnrtc__video_playout__) */
