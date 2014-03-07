//
//  video-decoder-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "frame-buffer.h"
#include "fetch-channel.h"
#include "params.h"
#include "ndnrtc-namespace.h"

using namespace ndnrtc::new_api;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new CocoaTestEnvironment);

class FetchChannelMock : public FetchChannel
{
public:
    virtual std::string getLogFile() const
    { return string(__FILE__)+".log"; }
    
    virtual ndnrtc::ParamsStruct
    getParameters() const { return params_; }
    
    void
    setParameters(ParamsStruct p)
    { params_ = p; }
    
private:
    ParamsStruct params_;
};

class FrameBufferTester : public ndnrtc::new_api::FrameBuffer
{
public:
    FrameBufferTester(shared_ptr<const FetchChannel> &fetchChannel):
    FrameBuffer(fetchChannel){}
    
};

class FrameBufferTests : public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        fetchChannel_.reset(new FetchChannelMock());
        buffer_ = new FrameBufferTester(fetchChannel_);
    }
    
    void TearDown()
    {
        delete buffer_;
        fetchChannel_.reset();
    }
    
    FrameBufferTester& getBuffer() { return *buffer_; }
    
protected:
    ParamsStruct params_;
    FrameBufferTester *buffer_;
    shared_ptr<const FetchChannel> fetchChannel_;
};

TEST_F(FrameBufferTests, TestAssembleFrames)
{
    int frameNo = 32;
    webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
    int nSegments = 7;
    int segmentSize = frame->_length/nSegments;
    
    params_ = DefaultParams;
    params_.segmentSize = segmentSize;
    ((FetchChannelMock*)fetchChannel_.get())->setParameters(params_);
    
    getBuffer().init();

    { // right most fetching
        ndnrtc::new_api::FrameBuffer::Event stateEvent =
        getBuffer().waitForEvents(ndnrtc::new_api::FrameBuffer::Event::StateChanged, 1000);
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Event::StateChanged, stateEvent.type_);

        
        Name prefix("/ndn/edu/ucla/apps/ndnrtc/user/testuser/streams/video0/vp8/frames/delta");
        Interest interest(prefix);
        interest.setChildSelector(1);
        interest.setNonce(NdnRtcUtils::nonceToBlob(NdnRtcUtils::generateNonceValue()));
        
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew,
                  getBuffer().interestIssued(interest));
        
        PacketData::PacketMetadata packetMeta;
        packetMeta.packetRate_ = 24;
        packetMeta.timestamp_ = NdnRtcUtils::millisecondTimestamp();
        
        PrefixMetaInfo prefixMeta;
        prefixMeta.keySequenceNo_ = 1;
        prefixMeta.playbackNo_ = 48;
        prefixMeta.totalSegmentsNum_ = -1; // will be added later inside packAndSlice call
        
        SegmentData::SegmentMetaInfo segmentMeta;
        segmentMeta.generationDelayMs_ = 100;
        segmentMeta.interestArrivalMs_ = NdnRtcUtils::millisecondTimestamp();
        segmentMeta.interestNonce_ = NdnRtcUtils::blobToNonce(interest.getNonce());
        
        std::vector<shared_ptr<Data>> segments =
        NdnRtcObjectTestHelper::packAndSliceFrame(frame, frameNo, segmentSize,
                                                  prefix, packetMeta,
                                                  prefixMeta, segmentMeta);
        // first segment arrived
        std::random_shuffle(segments.begin(), segments.end());
        std::vector<shared_ptr<Data>>::iterator it;
        
        int segCount = 0;
        for (it = segments.begin(); it != segments.end(); ++it)
        {
            // check last segment
            if (segCount == segments.size()-1)
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateReady,
                          getBuffer().newData(*(*it)));
            else
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateAssembling,
                          getBuffer().newData(*(*it)));
            
            if (segCount == 0)
            {
                // event for the 1st segment should be generated. check for that
                ndnrtc::new_api::FrameBuffer::Event event =
                getBuffer().waitForEvents(ndnrtc::new_api::FrameBuffer::Event::FirstSegment, 1000);
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Event::FirstSegment, event.type_);
                EXPECT_TRUE(ndnrtc::new_api::FrameBuffer::Slot::PrefixMeta&event.slot_->getConsistencyState());
                
                EXPECT_EQ(segments.size(), event.slot_->getSegmentsNumber());
                EXPECT_EQ(prefixMeta.playbackNo_, event.slot_->getPlaybackNumber());
                EXPECT_EQ(prefixMeta.keySequenceNo_, event.slot_->getGopKeyFrameNumber());
                
                PacketNumber segmentNo = NdnRtcNamespace::getSegmentNumber((*it)->getName());
                // now, issue the rest of the interests
                for (int i = 0; i < event.slot_->getSegmentsNumber(); i++)
                {
                    if (i != segmentNo)
                    {
                        Name segmentPrefix(prefix);
                        segmentPrefix.append(NdnRtcUtils::componentFromInt(frameNo));
                        segmentPrefix.appendSegment(i);

                        Interest interest(segmentPrefix);
                        interest.setChildSelector(1);
                        interest.setNonce(NdnRtcUtils::nonceToBlob(NdnRtcUtils::generateNonceValue()));
                        
                        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateAssembling,
                                  getBuffer().interestIssued(interest));
                    }
                }
            }

            segCount++;
        }
        
        // frame should be assembled - check it
        ndnrtc::new_api::FrameBuffer::Event event =
        getBuffer().waitForEvents(ndnrtc::new_api::FrameBuffer::Event::Ready, 1000);
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Event::Ready, event.type_);

        PacketData *packetData;
        EXPECT_EQ(RESULT_OK, event.slot_->getPacketData(&packetData));
        EXPECT_EQ(packetMeta.packetRate_, packetData->getMetadata().packetRate_);
        EXPECT_EQ(packetMeta.timestamp_, packetData->getMetadata().timestamp_);
        EXPECT_EQ(PacketData::TypeVideo, packetData->getType());
        EXPECT_TRUE(packetData->isValid());
        
        webrtc::EncodedImage restoredFrame;
        ((NdnFrameData*)packetData)->getFrame(restoredFrame);
        NdnRtcObjectTestHelper::checkFrames(frame, &restoredFrame);
    }
}
