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

class BufferTests : public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        basePrefix_ = Name(basePrefixString_);
        rightmostDeltaPrefix_ = Name(basePrefix_);
        rightmostDeltaPrefix_.append(NdnRtcNamespace::NameComponentStreamFramesDelta);
        rightmostKeyPrefix_ = Name(basePrefix_);
        rightmostKeyPrefix_.append(NdnRtcNamespace::NameComponentStreamFramesKey);
        
        setupData();
        initData(true);
    }
    
    void TearDown()
    {
        
    }
    
protected:
    std::string basePrefixString_ = "/ndn/edu/ucla/apps/ndnrtc/user/testuser/streams/video0/vp8/frames";
    Name basePrefix_, rightmostDeltaPrefix_, rightmostKeyPrefix_;
    
    PacketNumber frameNo_;
    Name framePrefix_;
    
    webrtc::EncodedImage *frame_;
    int nSegments_;
    int segmentSize_;
    double packetRate_;
    int64_t packetTimestamp_;
    PacketNumber keySeqNo_;
    PacketNumber playbackNo_;
    int32_t generationDelay_;
    int64_t interestArrivalMs_;
    int32_t interestNonceValue_;
    
    PacketData::PacketMetadata packetMeta_;
    PrefixMetaInfo prefixMeta_;
    SegmentData::SegmentMetaInfo segmentMeta_;
    
    std::vector<shared_ptr<Data>> segments_;
    
    void
    setupData()
    {
        frameNo_ = NdnRtcObjectTestHelper::randomInt(20, 100);
        frame_ = NdnRtcObjectTestHelper::loadEncodedFrame();
        nSegments_ = 7;
        packetRate_ = 28.7;
        generationDelay_ = NdnRtcObjectTestHelper::randomInt(5, 15);
        interestNonceValue_ = NdnRtcUtils::generateNonceValue();
    }
    
    void
    initData(bool shuffle)
    {
        framePrefix_ = Name(rightmostDeltaPrefix_);
        framePrefix_.append(NdnRtcUtils::componentFromInt(frameNo_));

        segmentSize_ = frame_->_length/nSegments_;

        packetTimestamp_ = NdnRtcUtils::millisecondTimestamp();
        keySeqNo_ = frameNo_/(int)ceil(packetRate_);
        playbackNo_ = frameNo_ + keySeqNo_;

        interestArrivalMs_ = packetTimestamp_-generationDelay_;
        
        packetMeta_ = {packetRate_, packetTimestamp_};
        prefixMeta_ = {-1, playbackNo_, keySeqNo_};
        segmentMeta_ = {interestNonceValue_, interestArrivalMs_, generationDelay_};
        
        segments_ =
        NdnRtcObjectTestHelper::packAndSliceFrame(frame_, frameNo_, segmentSize_,
                                                  rightmostDeltaPrefix_, packetMeta_,
                                                  prefixMeta_, segmentMeta_);
        
        if (shuffle)
            std::random_shuffle(segments_.begin(), segments_.end());
    }
    
};

class SegmentTester : public ndnrtc::new_api::FrameBuffer::Slot::Segment
{
public:
    SegmentTester() : Segment() {}
};

class SegmentTests : public BufferTests
{
public:
    void SetUp()
    {
        BufferTests::SetUp();
    }
    void TearDown()
    {
        BufferTests::TearDown();
    }
    
protected:
    void testSegmentIsFresh(SegmentTester& segment)
    {
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StateNotUsed,
                  segment.getState());
        EXPECT_EQ(-1, segment.getNumber());
        EXPECT_EQ(-1, segment.getMetadata().interestArrivalMs_);
        EXPECT_EQ(-1, segment.getMetadata().interestNonce_);
        EXPECT_EQ(-1, segment.getMetadata().generationDelayMs_);
        EXPECT_EQ(-1, segment.getPayloadSize());
        EXPECT_FALSE(segment.isOriginal());
        EXPECT_STREQ("/", segment.getPrefix().toUri().c_str());
    }
};

TEST_F(SegmentTests, TestInit)
{
    SegmentTester segment;
    
    testSegmentIsFresh(segment);
}

TEST_F(SegmentTests, TestSegmentOperations)
{
    {
        SegmentTester segment;
        uint32_t nonce = NdnRtcUtils::generateNonceValue();
        
        Interest i(rightmostDeltaPrefix_);
        i.setNonce(NdnRtcUtils::nonceToBlob(nonce));
        
        segment.interestIssued(NdnRtcUtils::blobToNonce(i.getNonce()));
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StatePending,
                  segment.getState());
        segment.markMissed();
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StateMissing,
                  segment.getState());
        
        segment.interestIssued(nonce);
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StatePending,
                  segment.getState());

        segment.discard();
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StateNotUsed,
                  segment.getState());
        
        segment.interestIssued(nonce);
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StatePending,
                  segment.getState());
        
        // test data arrivals
        PacketNumber packetNo = 45;
        SegmentNumber segNo = 4;
        Name segmentPrefix(rightmostDeltaPrefix_);
        segmentPrefix.append(NdnRtcUtils::componentFromInt(packetNo));
        segmentPrefix.appendSegment(segNo);
        
        { // non-original data arrival
            SegmentData::SegmentMetaInfo meta;
            meta.interestArrivalMs_ = NdnRtcObjectTestHelper::randomInt(0, INT_MAX);
            meta.generationDelayMs_ = 0;
            meta.interestNonce_ = NdnRtcUtils::generateNonceValue();
            
            segment.dataArrived(meta);
            
            EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StateFetched,
                      segment.getState());
            EXPECT_FALSE(segment.isOriginal());
            
            segment.setNumber(segNo);
            EXPECT_EQ(segNo, segment.getNumber());
            
            segment.setPrefix(segmentPrefix);
            EXPECT_STREQ(segmentPrefix.toUri().c_str(),
                         segment.getPrefix().toUri().c_str());
        }
        { // original data arrival
            SegmentData::SegmentMetaInfo meta;
            meta.interestArrivalMs_ = NdnRtcObjectTestHelper::randomInt(0, INT_MAX);
            meta.generationDelayMs_ = 20;
            meta.interestNonce_ = nonce;
            
            segment.dataArrived(meta);
            
            EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Segment::StateFetched,
                      segment.getState());
            EXPECT_TRUE(segment.isOriginal());
            EXPECT_EQ(meta.interestArrivalMs_, segment.getMetadata().interestArrivalMs_);
            EXPECT_EQ(meta.generationDelayMs_, segment.getMetadata().generationDelayMs_);
            EXPECT_EQ(meta.interestNonce_, segment.getMetadata().interestNonce_);
            
            segment.setNumber(segNo);
            EXPECT_EQ(segNo, segment.getNumber());
            
            segment.setPrefix(segmentPrefix);
            EXPECT_STREQ(segmentPrefix.toUri().c_str(),
                         segment.getPrefix().toUri().c_str());
            
            segment.discard();
            testSegmentIsFresh(segment);
        }
    }
}


class SlotTester : public ndnrtc::new_api::FrameBuffer::Slot
{
public:
    SlotTester(unsigned int segmentsize) : Slot(segmentsize){}
};

class SlotTests : public BufferTests
{
public:
    void SetUp()
    {
        BufferTests::SetUp();
    }
    
    void TearDown()
    {
        BufferTests::TearDown();
    }
    
protected:
    inline void testSlotIsFresh(ndnrtc::new_api::FrameBuffer::Slot& slot)
    {
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateFree, slot.getState());
        EXPECT_EQ(-1, slot.getPlaybackDeadline());
        EXPECT_EQ(-1, slot.getProducerTimestamp());
        EXPECT_EQ(-1, slot.getPacketRate());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Consistency::Inconsistent,
                  slot.getConsistencyState());
        EXPECT_EQ(-1, slot.getPlaybackNumber());
        EXPECT_EQ(-1, slot.getSequentialNumber());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Unknown,
                  slot.getNamespace());
        EXPECT_EQ(-1, slot.getGopKeyFrameNumber());
        EXPECT_EQ(0, slot.getSegmentsNumber());
        EXPECT_EQ(0, slot.getMissingSegments().size());
        EXPECT_EQ(0, slot.getAssembledLevel());
        EXPECT_STREQ(Name().toUri().c_str(), slot.getPrefix().toUri().c_str());
        EXPECT_EQ(NULL, slot.getSegment(0).get());
        EXPECT_EQ(NULL, slot.getRecentSegment().get());
        EXPECT_FALSE(slot.isRightmost());
    }
    
};

TEST_F(SlotTests, TestInit)
{
    ASSERT_NO_THROW(
                    SlotTester slot0(1);
                    testSlotIsFresh(slot0);
                    
                    SlotTester slot1(10);
                    testSlotIsFresh(slot1);
                    
                    SlotTester slot2(100);
                    testSlotIsFresh(slot2);
                    
                    SlotTester slot3(1000);
                    testSlotIsFresh(slot3);
                    
                    SlotTester slot4(10000);
                    testSlotIsFresh(slot4);
                    );
}

TEST_F(SlotTests, TestAddInterests)
{
    SlotTester slot(segmentSize_);
    
    // simply add rightmost
    {
        Interest i(rightmostDeltaPrefix_);
        
        testSlotIsFresh(slot);
        
        EXPECT_EQ(RESULT_OK, slot.addInterest(i));
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
        EXPECT_EQ(-1, slot.getPlaybackDeadline());
        EXPECT_EQ(-1, slot.getProducerTimestamp());
        EXPECT_EQ(-1, slot.getPacketRate());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Consistency::Inconsistent,
                  slot.getConsistencyState());
        EXPECT_EQ(-1, slot.getPlaybackNumber());
        EXPECT_EQ(-1, slot.getSequentialNumber());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Delta,
                  slot.getNamespace());
        EXPECT_EQ(-1, slot.getGopKeyFrameNumber());
        EXPECT_EQ(0, slot.getSegmentsNumber());
        EXPECT_EQ(0, slot.getMissingSegments().size());
        EXPECT_EQ(0, slot.getAssembledLevel());
        EXPECT_STREQ(rightmostDeltaPrefix_.toUri().c_str(),
                     slot.getPrefix().toUri().c_str());
        EXPECT_EQ(nullptr, slot.getSegment(0).get());
        EXPECT_EQ(nullptr, slot.getRecentSegment().get());
        EXPECT_TRUE(slot.isRightmost());
    }
    
    slot.reset();
    testSlotIsFresh(slot);
    
    // add with packet and segment number
    {
        PacketNumber packetNo = NdnRtcObjectTestHelper::randomInt(0, 100);
        SegmentNumber segNo = NdnRtcObjectTestHelper::randomInt(0, 10);
        Name segmentPrefix(rightmostDeltaPrefix_);
        segmentPrefix.append(NdnRtcUtils::componentFromInt(packetNo));
        segmentPrefix.appendSegment(segNo);
        
        Interest i(segmentPrefix);
        
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateFree, slot.getState());
        EXPECT_EQ(RESULT_OK, slot.addInterest(i));
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
        EXPECT_EQ(1, slot.getPendingSegments().size());
        EXPECT_EQ(-1, slot.getPlaybackDeadline());
        EXPECT_EQ(-1, slot.getProducerTimestamp());
        EXPECT_EQ(-1, slot.getPacketRate());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Consistency::Inconsistent,
                  slot.getConsistencyState());
        EXPECT_EQ(-1, slot.getPlaybackNumber());
        EXPECT_EQ(packetNo, slot.getSequentialNumber());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Delta,
                  slot.getNamespace());
        EXPECT_EQ(-1, slot.getGopKeyFrameNumber());
        EXPECT_EQ(0, slot.getSegmentsNumber());
        EXPECT_EQ(0, slot.getMissingSegments().size());
        EXPECT_EQ(0, slot.getAssembledLevel());
        
        Name packetPrefix;
        NdnRtcNamespace::trimSegmentNumber(segmentPrefix, packetPrefix);
        
        EXPECT_STREQ(packetPrefix.toUri().c_str(),
                     slot.getPrefix().toUri().c_str());
        EXPECT_EQ(NULL, slot.getSegment(0).get());
        EXPECT_EQ(NULL, slot.getRecentSegment().get());
        EXPECT_FALSE(slot.isRightmost());
    }
    
    slot.reset();
    testSlotIsFresh(slot);
    
    // adding multiple interests
    {
        PacketNumber packetNo = NdnRtcObjectTestHelper::randomInt(0, 100);
        int nSegments = NdnRtcObjectTestHelper::randomInt(5, 10);
        Name packetPrefix(rightmostDeltaPrefix_);
        packetPrefix.append(NdnRtcUtils::componentFromInt(packetNo));
        
        for (int i = 0; i < nSegments; i++)
        {
            SegmentNumber segNo = i;
            Name segmentPrefix(packetPrefix);
            segmentPrefix.appendSegment(segNo);
            
            if (i == 0)
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateFree, slot.getState());
            
            Interest ints(segmentPrefix);
            EXPECT_EQ(RESULT_OK, slot.addInterest(ints));
            EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
            EXPECT_EQ(0, slot.getAssembledLevel());
            EXPECT_EQ(0, slot.getMissingSegments().size());
            EXPECT_EQ(packetNo, slot.getSequentialNumber());
            
            EXPECT_EQ(i+1, slot.getPendingSegments().size());
        }
    }
    
    slot.reset();
    testSlotIsFresh(slot);
    
    // adding itnerests wrongly
    {
        PacketNumber packetNo = NdnRtcObjectTestHelper::randomInt(0, 100);
        int nSegments = NdnRtcObjectTestHelper::randomInt(5, 10);
        Name packetPrefix(rightmostDeltaPrefix_);
        packetPrefix.append(NdnRtcUtils::componentFromInt(packetNo));
        
        for (int i = 0; i < nSegments; i++)
        {
            SegmentNumber segNo = i;
            Name segmentPrefix(packetPrefix);
            segmentPrefix.appendSegment(segNo);
            
            if (i == 0)
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateFree, slot.getState());
            
            Interest ints(segmentPrefix);
            EXPECT_EQ(RESULT_OK, slot.addInterest(ints));
            
            EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
            EXPECT_EQ(0, slot.getAssembledLevel());
            EXPECT_EQ(0, slot.getMissingSegments().size());
            EXPECT_EQ(packetNo, slot.getSequentialNumber());
            
            // mess around
            int choice = NdnRtcObjectTestHelper::randomInt(0, 2);
            switch (choice) {
                    // add the same interest
                    // should be ignored
                case 0:
                {
                    EXPECT_EQ(RESULT_WARN, slot.addInterest(ints));
                    EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
                    EXPECT_EQ(0, slot.getAssembledLevel());
                    EXPECT_EQ(0, slot.getMissingSegments().size());
                    EXPECT_EQ(i+1, slot.getPendingSegments().size());
                    EXPECT_EQ(0, slot.getFetchedSegments().size());
                    EXPECT_EQ(packetNo, slot.getSequentialNumber());
                }
                    break;
                    // add wrong interest
                    // result should be errored
                case 1:
                {
                    Name wrongPrefix(rightmostDeltaPrefix_);
                    PacketNumber wrongPacketNo = packetNo+1;
                    SegmentNumber wrongSegmentNo = segNo+1;
                    wrongPrefix.append(NdnRtcUtils::componentFromInt(wrongPacketNo));
                    wrongPrefix.appendSegment(segNo);
                    
                    EXPECT_EQ(RESULT_ERR, slot.addInterest(Interest(wrongPrefix)));
                    EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
                    EXPECT_EQ(0, slot.getAssembledLevel());
                    EXPECT_EQ(0, slot.getMissingSegments().size());
                    EXPECT_EQ(i+1, slot.getPendingSegments().size());
                    EXPECT_EQ(0, slot.getFetchedSegments().size());
                    EXPECT_EQ(packetNo, slot.getSequentialNumber());
                }
                    break;
                    
                    // add rightmost interest
                    // result should be errored
                case 2:
                {
                    EXPECT_EQ(RESULT_ERR, slot.addInterest(Interest(rightmostDeltaPrefix_)));
                    EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateNew, slot.getState());
                    EXPECT_EQ(0, slot.getAssembledLevel());
                    EXPECT_EQ(0, slot.getMissingSegments().size());
                    EXPECT_EQ(i+1, slot.getPendingSegments().size());
                    EXPECT_EQ(0, slot.getFetchedSegments().size());
                    EXPECT_EQ(packetNo, slot.getSequentialNumber());
                }
                    break;
                default:
                    break;
            }
            
        }
    }
}

TEST_F(SlotTests, TestAddInterestsAndData)
{
    setupData();
    initData(true);
    SlotTester slot(segmentSize_);
    
    // add rightmost - reply with data - add rest interests - reply with the rest data
    {
        Interest interest(rightmostDeltaPrefix_);
        interest.setNonce(NdnRtcUtils::nonceToBlob(interestNonceValue_));
        
        testSlotIsFresh(slot);
        slot.addInterest(interest);
        
        EXPECT_EQ(-1, slot.getPlaybackDeadline());
        EXPECT_EQ(-1, slot.getProducerTimestamp());
        EXPECT_EQ(-1, slot.getPacketRate());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Consistency::Inconsistent,
                  slot.getConsistencyState());
        EXPECT_EQ(-1, slot.getPlaybackNumber());
        EXPECT_EQ(-1, slot.getSequentialNumber());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Delta,
                  slot.getNamespace());
        EXPECT_EQ(-1, slot.getGopKeyFrameNumber());
        EXPECT_EQ(0, slot.getSegmentsNumber());
        EXPECT_EQ(0, slot.getMissingSegments().size());
        EXPECT_EQ(0, slot.getAssembledLevel());
        EXPECT_STREQ(rightmostDeltaPrefix_.toUri().c_str(),
                     slot.getPrefix().toUri().c_str());
        EXPECT_EQ(NULL, slot.getSegment(0).get());
        EXPECT_EQ(NULL, slot.getRecentSegment().get());
        EXPECT_TRUE(slot.isRightmost());
        
        std::vector<shared_ptr<Data>>::iterator it = segments_.begin();
        int i = 0;
        
        while (it != segments_.end())
        {
            shared_ptr<Data> data = *it;
            
            ndnrtc::new_api::FrameBuffer::Slot::State
            newState = slot.appendData(*data);
            
            ASSERT_NE(nullptr, slot.getRecentSegment().get());
            
            PacketNumber dataPacketNo = NdnRtcNamespace::getPacketNumber(data->getName());
            SegmentNumber dataSegNo = NdnRtcNamespace::getSegmentNumber(data->getName());
            EXPECT_EQ(dataSegNo, slot.getRecentSegment()->getNumber());

            Name dataPrefix;
            NdnRtcNamespace::trimSegmentNumber(data->getName(), dataPrefix);
            dataPrefix.appendSegment(dataSegNo);
            EXPECT_STREQ(dataPrefix.toUri().c_str(),
                         slot.getRecentSegment()->getPrefix().toUri().c_str());
            
            EXPECT_EQ(i+1, slot.getFetchedSegments().size());
            EXPECT_EQ((double)(i+1)/(double)segments_.size(), slot.getAssembledLevel());
            
            if (i == 0)
            {
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::State::StateAssembling,
                          newState);
                EXPECT_TRUE(ndnrtc::new_api::FrameBuffer::Slot::Consistency::PrefixMeta&
                          slot.getConsistencyState());
                EXPECT_EQ(frameNo_, slot.getSequentialNumber());
                EXPECT_EQ(keySeqNo_, slot.getGopKeyFrameNumber());
                EXPECT_EQ(segments_.size()-1, slot.getMissingSegments().size());
                EXPECT_EQ(0, slot.getPendingSegments().size());
                EXPECT_EQ(segments_.size(), slot.getSegmentsNumber());
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::Delta,
                          slot.getNamespace());
                
                // now issue the rest
                for (int seg = 0; seg < segments_.size(); seg++)
                {
                    if (seg != dataSegNo)
                    {
                        Name segmentPrefix(framePrefix_);
                        segmentPrefix.appendSegment(seg);
                        
                        EXPECT_EQ(RESULT_OK, slot.addInterest(Interest(segmentPrefix)));

                        int expectedPendingSegments = (seg > dataSegNo)?seg:seg+1;
                        
                        EXPECT_EQ(expectedPendingSegments, slot.getPendingSegments().size());
                        EXPECT_EQ(slot.getSegmentsNumber()-expectedPendingSegments-1,
                                  slot.getMissingSegments().size());
                        
                    }
                }
            }
            
            if (i == segments_.size()-1)
            {
                EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::State::StateReady,
                          newState);
                EXPECT_EQ(0, slot.getPendingSegments().size());
                EXPECT_EQ(0, slot.getMissingSegments().size());
            }
            
            if (slot.getRecentSegment()->getNumber() == 0)
            {
                EXPECT_TRUE(ndnrtc::new_api::FrameBuffer::Slot::Consistency::HeaderMeta&
                            slot.getConsistencyState());
                EXPECT_EQ(packetRate_, slot.getPacketRate());
                EXPECT_EQ(packetTimestamp_, slot.getProducerTimestamp());
            }
                          
            ++it;
            ++i;
        }
        
        PacketData *frameData;
        slot.getPacketData(&frameData);
        
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, ((NdnFrameData*)frameData)->getFrame(restoredFrame));
        NdnRtcObjectTestHelper::checkFrames(frame_, &restoredFrame);
    }
    
    slot.reset();
    testSlotIsFresh(slot);
    
    // add rightmost - reply with data - add rest interests -
    // reply with some data - mark missing - re-express interests - add rest data
    {
        Interest interest(rightmostDeltaPrefix_);
        interest.setNonce(NdnRtcUtils::nonceToBlob(interestNonceValue_));

        slot.addInterest(interest);
        std::vector<shared_ptr<Data>>::iterator it = segments_.begin();
        int i = 0;
        int missingTrigger = NdnRtcObjectTestHelper::randomInt(segments_.size()/2,
                                                               segments_.size()-2);
        
        while (it != segments_.end())
        {
            shared_ptr<Data> data = *it;

            PacketNumber dataPacketNo = NdnRtcNamespace::getPacketNumber(data->getName());
            SegmentNumber dataSegNo = NdnRtcNamespace::getSegmentNumber(data->getName());

            ndnrtc::new_api::FrameBuffer::Slot::State
            newState = slot.appendData(*data);
            if (i == 0)
            {
                // now issue the rest
                for (int seg = 0; seg < segments_.size(); seg++)
                {
                    if (seg != dataSegNo)
                    {
                        Name segmentPrefix(framePrefix_);
                        segmentPrefix.appendSegment(seg);
                        EXPECT_EQ(RESULT_OK, slot.addInterest(Interest(segmentPrefix)));
                    }
                } // for
            }// i == 0
            
            if (i == missingTrigger)
            {
                int testErrorN = NdnRtcObjectTestHelper::randomInt(1, 5);
                // now mark rest of the interests as missing
                for (int seg = 0; seg < segments_.size()+testErrorN; seg++)
                {
                    int oldMissingNum = slot.getMissingSegments().size();
                    
                    if (seg < segments_.size())
                    {
                        SegmentNumber segNo = NdnRtcNamespace::getSegmentNumber(segments_[seg]->getName());
                        
                        Name segmentPrefix(framePrefix_);
                        segmentPrefix.appendSegment(segNo);

                        if (seg > missingTrigger)
                        {
                            EXPECT_EQ(RESULT_OK, slot.markMissing(Interest(segmentPrefix)));
                            EXPECT_EQ(oldMissingNum+1, slot.getMissingSegments().size());
                            
                            // re-express
                            EXPECT_EQ(RESULT_OK, slot.addInterest(Interest(segmentPrefix)));
                        }
                        else
                        {
                            EXPECT_EQ(RESULT_WARN, slot.markMissing(Interest(segmentPrefix)));
                            EXPECT_EQ(oldMissingNum, slot.getMissingSegments().size());
                        }
                    }
                    else
                    {
                        Name segmentPrefix(framePrefix_);
                        segmentPrefix.appendSegment(seg);
                        
                        EXPECT_EQ(RESULT_ERR, slot.markMissing(Interest(segmentPrefix)));
                        EXPECT_EQ(oldMissingNum, slot.getMissingSegments().size());
                    }
                } // for
            } // if
            
            ++it;
            ++i;
        } // while
        
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::State::StateReady,
                  slot.getState());

        PacketData *frameData;
        slot.getPacketData(&frameData);
        
        webrtc::EncodedImage restoredFrame;
        EXPECT_EQ(RESULT_OK, ((NdnFrameData*)frameData)->getFrame(restoredFrame));
        NdnRtcObjectTestHelper::checkFrames(frame_, &restoredFrame);
    }
}

TEST_F(SlotTests, TestVariousCalls)
{
    {
        Data d;
        SlotTester slot(segmentSize_);
        
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateFree, slot.appendData(d));
    }
    // test lock/unlock
    {
        initData(true);
        setupData();
        
        SlotTester slot(segmentSize_);
        
        for (int i = 0; i < segments_.size(); i++)
        {
            Name segmentPrefix(framePrefix_);
            segmentPrefix.appendSegment(i);
            
            EXPECT_EQ(RESULT_OK,slot.addInterest(Interest(segmentPrefix)));
        }
        
        for (int i = 0; i < segments_.size(); i++)
        {
            slot.appendData(*segments_[i]);
        }
        
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateReady,
                  slot.getState());
        
        // lock
        slot.lock();
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateLocked,
                  slot.getState());
        
        EXPECT_EQ(RESULT_ERR, slot.reset());
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateLocked,
                  slot.getState());
        
        Name segPrefix(framePrefix_);
        segPrefix.appendSegment(0);
        EXPECT_EQ(RESULT_ERR,
                  slot.addInterest(Interest(segPrefix)));
        EXPECT_EQ(RESULT_ERR,
                  slot.markMissing(Interest(segPrefix)));
        
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateLocked,
                  slot.appendData(*segments_[0]));
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateLocked,
                  slot.getState());
        
        slot.unlock();
        EXPECT_EQ(ndnrtc::new_api::FrameBuffer::Slot::StateReady,
                  slot.getState());
        
    }

    // mark missing rightmost
    {
        SlotTester slot(segmentSize_);
        slot.addInterest(Interest(rightmostDeltaPrefix_));
        
        EXPECT_EQ(RESULT_OK, slot.markMissing(Interest(rightmostDeltaPrefix_)));
        EXPECT_EQ(1, slot.getMissingSegments().size());
    }
    
    // use missing for enumeration
    {
        SlotTester slot(segmentSize_);
        
        // NOTE: nSegments_ is always less than actual number of segments
        // (segments_.size())
        
        // issue until nSegments_
        for (int i = 0; i < nSegments_; i++)
        {
            Name segmentPrefix(framePrefix_);
            segmentPrefix.appendSegment(i);
            
            EXPECT_EQ(RESULT_OK, slot.addInterest(Interest(segmentPrefix)));
        }
        
        for (int i = 0; i < segments_.size(); i++)
        {
            slot.appendData(*segments_[i]);
            
            // now should have received total segments number
            if (i == 0)
            {
                EXPECT_NE(nSegments_, slot.getSegmentsNumber());
                
                std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>::iterator it;
                std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>> missing;
                missing = slot.getMissingSegments();
                
                EXPECT_LT(0, missing.size());
                
                for (it = missing.begin(); it != missing.end(); ++it)
                {
                    EXPECT_EQ(RESULT_OK, slot.addInterest((*it)->getPrefix()));
                }
            }
        }
    }
    // use missing for enumeration with rightmost
    {
        SlotTester slot(segmentSize_);
        
        // NOTE: nSegments_ is always less than actual number of segments
        // (segments_.size())
        
        slot.addInterest(Interest(rightmostDeltaPrefix_));
        slot.appendData(*segments_[0]);
        
        EXPECT_NE(nSegments_, slot.getSegmentsNumber());
        
        std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>::iterator it;
        std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>> missing;
        missing = slot.getMissingSegments();
        
        EXPECT_LT(0, missing.size());
        
        for (it = missing.begin(); it != missing.end(); ++it)
        {
            EXPECT_EQ(RESULT_OK, slot.addInterest((*it)->getPrefix()));
        }
    }
}

class FrameBufferTester : public ndnrtc::new_api::FrameBuffer
{
public:
    FrameBufferTester(shared_ptr<const FetchChannel> &fetchChannel):
    FrameBuffer(fetchChannel){}
    
};

class FrameBufferTests : public NdnRtcObjectTestHelper
{
public:
    FrameBufferTests():
    interestArrivedEvent_(*webrtc::EventWrapper::Create()),
    accessCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection())
    {}
    
    void SetUp()
    {
        fetchChannel_.reset(new FetchChannelMock(ENV_LOGFILE));
        ((FetchChannelMock*)fetchChannel_.get())->setParameters(params_);
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
    
    webrtc::EventWrapper& interestArrivedEvent_;
    webrtc::CriticalSectionWrapper& accessCs_;
    std::vector<shared_ptr<Interest>> pendingInterests_;
    bool isRunning_;
    
    static bool
    segmentProviderRoutine(void* obj)
    {
        return ((FrameBufferTests*)obj)->provideSegments();
    }
    bool
    provideSegments()
    {
        accessCs_.Enter();
        while (pendingInterests_.size() > 0)
        {
            shared_ptr<Interest> interest = pendingInterests_.front();
            pendingInterests_.erase(pendingInterests_.begin());
            
            shared_ptr<Data> data = getData(*interest);
            
            if (data.get())
            {
                getBuffer().newData(*data);
            }
        }
        accessCs_.Leave();
        
        interestArrivedEvent_.Wait(WEBRTC_EVENT_INFINITE);
        return isRunning_;
    }
    
    std::map<PacketNumber, std::vector<shared_ptr<Data>>> packets_;
    
    void
    prepareData(PacketNumber startPacketNo, PacketNumber endPacketNo,
                int nSegments, Name prefix, bool isShuffled)
    {
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        int segmentSize = frame->_length/nSegments;
        int64_t startTime = NdnRtcUtils::millisecondTimestamp();
        int i = 0;
        unsigned int frameDuration = 1000./params_.producerRate;
        PacketNumber lastKey = 0;
        
        for (PacketNumber pNo = startPacketNo; pNo <= endPacketNo; pNo++)
        {
            bool isKey = false; // (0 == pNo%(int)params_.producerRate);
            
            if (isKey)
            {
                pNo = pNo/(int)params_.producerRate;
                lastKey = pNo;
            }
            
            frame->_frameType = isKey ? webrtc::kKeyFrame : webrtc::kDeltaFrame;
            
            int timeDelta = NdnRtcObjectTestHelper::randomInt(0, 5);
            frame->capture_time_ms_ = startTime+i*frameDuration+timeDelta;
            
            PacketData::PacketMetadata packetMeta;
            packetMeta.packetRate_ = params_.producerRate;
            packetMeta.timestamp_ = frame->capture_time_ms_;
            
            PrefixMetaInfo prefixMeta;
            prefixMeta.keySequenceNo_ = lastKey;
            prefixMeta.playbackNo_ = pNo;
            prefixMeta.totalSegmentsNum_ = -1; // will be added later inside packAndSlice call
            
            SegmentData::SegmentMetaInfo segmentMeta;
            
            Name packetPrefix(prefix);
            
            if (isKey)
                packetPrefix.append(NdnRtcNamespace::NameComponentStreamFramesKey);
            else
                packetPrefix.append(NdnRtcNamespace::NameComponentStreamFramesDelta);
            
            std::vector<shared_ptr<Data>> segments =
            NdnRtcObjectTestHelper::packAndSliceFrame(frame, pNo, segmentSize,
                                                      packetPrefix, packetMeta,
                                                      prefixMeta, segmentMeta);
            packets_[pNo] = segments;

            if (isShuffled)
                std::random_shuffle(packets_[pNo].begin(), packets_[pNo].end());
            i++;
        }
    }
    
    shared_ptr<Data>
    getData(const Interest& interest)
    {
        shared_ptr<Data> lookupData(NULL);
        PacketNumber pNo = NdnRtcNamespace::getPacketNumber(interest.getName());
        std::vector<shared_ptr<Data>>* segments;
        
        if (pNo == -1)
        {
            pNo = packets_.begin()->first;
            segments = &(packets_[pNo]);
            if (segments->size())
            {
                lookupData = segments->operator[](0);
                segments->erase(segments->begin());
            }
        }
        else
        {
            segments = &(packets_[pNo]);
            std::vector<shared_ptr<Data>>::iterator it = segments->begin();
            bool found = false;
            
            while (it != segments->end())
            {
                Name interestPrefix = interest.getName();
                Name dataPrefix = (*it)->getName();
                
                if (interestPrefix.match(dataPrefix))
                {
                    found = true;
                    // remove this segment
                    lookupData = *it;
                    segments->erase(it);
                    break;
                }
                ++it;
            }
        }
        
        if (segments->size() == 0)
            packets_.erase(pNo);
        
        return lookupData;
    }
    
    class PipelinerTask {
    public:
        FrameBufferTests *obj_;
        PacketNumber startNo_;
        PacketNumber endNo_;
        Name prefix_;
        int nSegDelta_, nSegKey_;
        int sleepTimeMs_;
    };

    static bool
    pipelineFrames(void* obj)
    {
        PipelinerTask *task = (PipelinerTask*)obj;
        
        PacketNumber pNo = task->startNo_;
        while (pNo != task->endNo_+1)
        {
            bool isKey = false; //(pNo%(int)task->obj_->params_.producerRate == 0);
            task->obj_->issueInterests(task->prefix_, pNo, 0, (isKey)?task->nSegKey_:task->nSegDelta_, isKey);
            pNo++;
            usleep(task->sleepTimeMs_*1000);
        }
        
        delete task;
        return false;
    }
    
    void issueInterests(Name prefix, int frameNo, int startSegNo, int endSegNo, bool isKey = false)
    {
        Name framePrefix = prefix;
        
        if (isKey)
        {
            framePrefix.append(NdnRtcNamespace::NameComponentStreamFramesKey);
            frameNo /= (int)params_.producerRate;
        }
        else
            framePrefix.append(NdnRtcNamespace::NameComponentStreamFramesDelta);
        
        framePrefix.append(NdnRtcUtils::componentFromInt(frameNo));
        
        for (int segno = startSegNo; segno <= endSegNo; segno++)
        {
            Name segmentPrefix(framePrefix);
            segmentPrefix.appendSegment(segno);
            
            shared_ptr<Interest> interest(new Interest(segmentPrefix));
            
            if (!hasInterest(interest))
                issueInterest(interest);
        }
    }
    
    void issueInterest(const shared_ptr<Interest>& interest)
    {
        accessCs_.Enter();
        getBuffer().interestIssued(*interest);
        pendingInterests_.push_back(interest);
        accessCs_.Leave();
        
        interestArrivedEvent_.Set();
    }
    
    bool hasInterest(shared_ptr<Interest>& interest)
    {
        webrtc::CriticalSectionScoped scopedCs(&accessCs_);
        
        if (pendingInterests_.size() == 0)
            return false;
        
        std::vector<shared_ptr<Interest>>::iterator it = pendingInterests_.begin();
        
        while (it != pendingInterests_.end())
        {
            if ((*it)->getName() == interest->getName())
                return true;
            
            ++it;
        }
        
        return false;
    }
    
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
        delete packetData;
    }
}

TEST_F(FrameBufferTests, TestAssembleManyFrames)
{
    webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
    int nSegments = 7;
    int segmentSize = frame->_length/nSegments;
    
    params_ = DefaultParams;
    params_.producerRate = 30;
    params_.segmentSize = segmentSize;
    ((FetchChannelMock*)fetchChannel_.get())->setParameters(params_);
    getBuffer().init();
    
    Name prefix("/ndn/edu/ucla/apps/ndnrtc/user/testuser/streams/video0/vp8/frames");
    PacketNumber startPno = 1, endPno = 30;
    prepareData(startPno, endPno, nSegments, prefix, true);
    
    {
        webrtc::ThreadWrapper& providerThread = *webrtc::ThreadWrapper::CreateThread(FrameBufferTests::segmentProviderRoutine, this);
        webrtc::ThreadWrapper* pipelinerThread;
        isRunning_ = true;
        unsigned int tid;
        providerThread.Start(tid);
        bool stop = (packets_.size() == 0);
        int bufferEventsMask = ndnrtc::new_api::FrameBuffer::Event::StateChanged |
        ndnrtc::new_api::FrameBuffer::Event::FirstSegment |
        ndnrtc::new_api::FrameBuffer::Event::Ready;
        bool gotFirstSegment = false;
        int nReadySlots_ = 0;
        int startTimeMs = NdnRtcUtils::millisecondTimestamp();
        int waitTimeMs = 5000;
        
        while (!stop)
        {
            ndnrtc::new_api::FrameBuffer::Event event = getBuffer().waitForEvents(bufferEventsMask,1000);
            
            switch (event.type_) {
                case ndnrtc::new_api::FrameBuffer::Event::StateChanged:
                {
                    // issue rightmost
                    Name rmpref(prefix);
                    rmpref.append(NdnRtcNamespace::NameComponentStreamFramesDelta);
                    shared_ptr<Interest> rightmost(new Interest(rmpref));
                    rightmost->setChildSelector(1);
                    
                    issueInterest(rightmost);
                    EXPECT_EQ(1, getBuffer().getActiveSlots().size());
                }
                    break;
                    
                case ndnrtc::new_api::FrameBuffer::Event::FirstSegment:
                {
                    Name dataPrefix = event.slot_->getPrefix();
                    
                    {
                        if (!gotFirstSegment)
                        {
                            gotFirstSegment = true;
                            
                            PipelinerTask *task = new PipelinerTask();
                            task->obj_ = this;
                            task->prefix_ = prefix;
                            task->nSegDelta_ = nSegments;
                            task->nSegKey_ = nSegments;
                            task->startNo_ = event.slot_->getSequentialNumber()+1;
                            task->endNo_ = endPno;
                            task->sleepTimeMs_ = 10;
                            
                            pipelinerThread = webrtc::ThreadWrapper::CreateThread(FrameBufferTests::pipelineFrames, task);
                            unsigned int tid;
                            pipelinerThread->Start(tid);
                        }
                        
                        { // issue for missing segments
                            std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>> missing = event.slot_->getMissingSegments();
                            std::vector<shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment>>::iterator it;
                            
                            for (it = missing.begin(); it != missing.end(); ++it)
                            {
                                shared_ptr<ndnrtc::new_api::FrameBuffer::Slot::Segment> segment = *it;
                                shared_ptr<Interest> interest(new Interest(segment->getPrefix()));
                                
                                if (!hasInterest(interest))
                                    issueInterest(interest);
                            }
                        }
                    }
                    
                }
                    break;
                    
                case ndnrtc::new_api::FrameBuffer::Event::Ready:
                {
                    nReadySlots_++;
                }
                    break;
                default:
                    break;
            }
            
            stop = ((packets_.size() == 0) ||
                    (NdnRtcUtils::millisecondTimestamp()-startTimeMs > waitTimeMs));
        }
        
        EXPECT_EQ((endPno-startPno)+1,nReadySlots_);
        isRunning_ = false;
        interestArrivedEvent_.Set();
        providerThread.Stop();
        pipelinerThread->Stop();
    }
}

TEST_F(FrameBufferTests, TestPlaybackQueue)
{
    webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();

    int nSegments = 7;
    int segmentSize = frame->_length/nSegments;
    
    params_ = DefaultParams;
    params_.producerRate = 30.;
    params_.segmentSize = segmentSize;
    ((FetchChannelMock*)fetchChannel_.get())->setParameters(params_);
    
    getBuffer().init();

    int64_t startTime = NdnRtcUtils::millisecondTimestamp();
    unsigned int rightMostDuration = 34, delta1Duration = 32, delta2Duration = 30;
    
    PacketData::PacketMetadata packetMeta;
    packetMeta.packetRate_ = 30;
    packetMeta.timestamp_ = startTime+rightMostDuration;
    
    //---
    PrefixMetaInfo rightmostPrefixMeta;
    rightmostPrefixMeta.playbackNo_ = 1;
    rightmostPrefixMeta.keySequenceNo_ = 0;
    
    SegmentData::SegmentMetaInfo rightmostSegmentMeta;
    rightmostSegmentMeta.generationDelayMs_ = 10;
    rightmostSegmentMeta.interestNonce_ = 0;
    rightmostSegmentMeta.interestArrivalMs_ = NdnRtcUtils::millisecondTimestamp();
    
    Name rightmost("/ndn/edu/ucla/apps/ndnrtc/user/testuser/streams/video0/vp8/frames");
    Name rightmostDataPrefix(rightmost);
    rightmostDataPrefix.append(NdnRtcNamespace::NameComponentStreamFramesDelta);
    
    std::vector<shared_ptr<Data>> rightmostSegments =
    NdnRtcObjectTestHelper::packAndSliceFrame(frame, 0, segmentSize,
                                              rightmostDataPrefix, packetMeta,
                                              rightmostPrefixMeta, rightmostSegmentMeta);


    Name deltaPrefix("/ndn/edu/ucla/apps/ndnrtc/user/testuser/streams/video0/vp8/frames/delta");
    PrefixMetaInfo deltaPrefixMeta;
    deltaPrefixMeta.playbackNo_ = rightmostPrefixMeta.playbackNo_+1;
    deltaPrefixMeta.keySequenceNo_ = 0;
    
    packetMeta.timestamp_ += delta1Duration;
    
    SegmentData::SegmentMetaInfo deltaSegmentMeta;// ignore
    std::vector<shared_ptr<Data>> delta1Segments =
    NdnRtcObjectTestHelper::packAndSliceFrame(frame, 1, segmentSize,
                                              deltaPrefix, packetMeta,
                                              deltaPrefixMeta, deltaSegmentMeta);

    
    packetMeta.timestamp_ += delta2Duration;
    deltaPrefixMeta.playbackNo_++;
    std::vector<shared_ptr<Data>> delta2Segments =
    NdnRtcObjectTestHelper::packAndSliceFrame(frame, 2, segmentSize,
                                              deltaPrefix, packetMeta,
                                              deltaPrefixMeta, deltaSegmentMeta);

    
    Name keyPrefix("/ndn/edu/ucla/apps/ndnrtc/user/testuser/streams/video0/vp8/frames/key");
    PrefixMetaInfo keyPrefixMeta;
    keyPrefixMeta.playbackNo_ = 0;
    keyPrefixMeta.keySequenceNo_ = 0;
    
    packetMeta.timestamp_ = startTime;
    
    SegmentData::SegmentMetaInfo keySegmentMeta; // ignore
    
    
    std::vector<shared_ptr<Data>> keySegments =
    NdnRtcObjectTestHelper::packAndSliceFrame(frame, 0, segmentSize,
                                              keyPrefix, packetMeta,
                                              keyPrefixMeta, keySegmentMeta);
    
    // now simulate adding frames into buffer and monitor it's size
    EXPECT_EQ(0, getBuffer().getEstimatedBufferSize());
    
    { // scenario 1
        unsigned int frameDuration = ceil(1000/params_.producerRate);
        
      // issue for rightmost
        Interest rightmostInterest(deltaPrefix);
        rightmostInterest.setChildSelector(1);
        rightmostInterest.setNonce(NdnRtcUtils::nonceToBlob(NdnRtcUtils::generateNonceValue()));
        
        getBuffer().interestIssued(rightmostInterest);
        EXPECT_EQ(frameDuration, getBuffer().getEstimatedBufferSize());
        EXPECT_EQ(1, getBuffer().getActiveSlots().size());
        
        // now append 1st rightmost non-zero segment
        getBuffer().newData(*rightmostSegments[1]);
        EXPECT_EQ(ceil(1000/params_.producerRate), getBuffer().getEstimatedBufferSize());
        
        // now issue for 2 more delta frames
        Name delta1Name(deltaPrefix);
        delta1Name.append(NdnRtcUtils::componentFromInt(1));
        getBuffer().interestIssued(Interest(delta1Name));
        EXPECT_EQ(2, getBuffer().getActiveSlots().size());
        
        EXPECT_EQ(2*frameDuration, getBuffer().getEstimatedBufferSize());
        
        Name delta2Name(deltaPrefix);
        delta2Name.append(NdnRtcUtils::componentFromInt(2));
        getBuffer().interestIssued(Interest(delta2Name));
        EXPECT_EQ(3, getBuffer().getActiveSlots().size());
        
        // append non-zero segments
        getBuffer().newData(*delta1Segments[1]);
        
        // now size should be inferred from provided producer rate for 2 frames
        EXPECT_EQ(3*frameDuration, getBuffer().getEstimatedBufferSize());

        // append zero segment (which contain actual timestamps)
        Interest rmseg0(deltaPrefix);
        rmseg0.getName().append(NdnRtcUtils::componentFromInt(0));
        rmseg0.getName().appendSegment(0);
        getBuffer().interestIssued(rmseg0);
        
        Interest delta1seg0(deltaPrefix);
        delta1seg0.getName().append(NdnRtcUtils::componentFromInt(1));
        delta1seg0.getName().appendSegment(0);
        getBuffer().interestIssued(delta1seg0);
        
        getBuffer().newData(*delta1Segments[0]);
        getBuffer().newData(*rightmostSegments[0]);
        
        // now first frame has precise duration
        EXPECT_EQ(rightMostDuration+2*frameDuration, getBuffer().getEstimatedBufferSize());
    }
}
