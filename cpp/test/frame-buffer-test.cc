//
//  frame-buffer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//
#include <tr1/unordered_set>

#include "test-common.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "ndnrtc-namespace.h"


using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

#if 0
//********************************************************************************
// Slot tests
class FrameSlotTester : public NdnRtcObjectTestHelper
{
    public :
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        encodedFrame_ = NdnRtcObjectTestHelper::loadEncodedFrame();
        ASSERT_NE((webrtc::EncodedImage*)NULL, encodedFrame_);
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        if (encodedFrame_)
        {
            delete encodedFrame_->_buffer;
            delete encodedFrame_;
        }
    }
    
protected:
    webrtc::EncodedImage *encodedFrame_;
    
};

TEST_F(FrameSlotTester, CreateDelete)
{
    FrameBuffer::Slot * slot = new FrameBuffer::Slot(4000);
    EXPECT_EQ(INT_MAX, slot->getFrameNumber());
    delete slot;
}

TEST_F(FrameSlotTester, TestStates)
{
    FrameBuffer::Slot * slot = new FrameBuffer::Slot(4000);
    
    EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
    
    slot->markNew(123, false);
    EXPECT_EQ(FrameBuffer::Slot::StateNew, slot->getState());
    EXPECT_EQ(123, slot->getBookingId());
    
    slot->markAssembling(3, 100);
    EXPECT_EQ(FrameBuffer::Slot::StateAssembling, slot->getState());
    
    slot->markLocked();
    EXPECT_EQ(FrameBuffer::Slot::StateLocked, slot->getState());
    
    slot->markFree();
    EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
    
    delete slot;
}

TEST_F(FrameSlotTester, SimpleAssemble)
{
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int slotSize = 4000; // big enough to store encoded frame?
    unsigned int segmentsNum = 3;
    unsigned int segmentSize = encodedFrame_->_length / segmentsNum;
    
    vector<unsigned char*> segments;
    unsigned int lastSegmentSize = frameData.getLength() - segmentsNum*segmentSize;
    
    // slice data into segments
    unsigned int actualSegmentsNum = (lastSegmentSize)?segmentsNum+1:segmentsNum;
    unsigned char *segmentPtr;
    
    for (int i = 0; i < actualSegmentsNum; i++)
    {
        segmentPtr = frameData.getData()+i*segmentSize;
        segments.push_back(segmentPtr);
    }
    
    {
        FrameBuffer::Slot * slot = new FrameBuffer::Slot(slotSize);
        
        EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
        slot->markNew(0, false);
        slot->markAssembling(actualSegmentsNum, segmentSize);
        
        for (int i = 0; i < actualSegmentsNum; i++)
        {
            unsigned int size = (i == actualSegmentsNum-1)?lastSegmentSize:segmentSize;
            
            FrameBuffer::Slot::State state = slot->appendSegment(i, size, segments[i]);
            
            if (i == actualSegmentsNum-1)
                EXPECT_EQ(FrameBuffer::Slot::StateReady, state);
            else
                EXPECT_EQ(FrameBuffer::Slot::StateAssembling, state);
        }
        
        // check frame consistency
        shared_ptr<webrtc::EncodedImage> frame = slot->getFrame();
        
        EXPECT_NE(nullptr, frame.get());
        NdnRtcObjectTestHelper::checkFrames(encodedFrame_, frame.get());
        
        delete slot;
    }
}

// tests assembing in wrong order
TEST_F(FrameSlotTester, RandomAssemble)
{
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int slotSize = 4000; // big enough to store encoded frame?
    unsigned int segmentsNum = 3;
    unsigned int segmentSize = encodedFrame_->_length / segmentsNum;
    
    vector<unsigned char*> segments;
    unsigned int lastSegmentSize = frameData.getLength() - segmentsNum*segmentSize;
    
    // slice data into segments
    unsigned int actualSegmentsNum = (lastSegmentSize)?segmentsNum+1:segmentsNum;
    unsigned char *segmentPtr;
    
    vector<int> segmentsIdx;
    
    for (int i = 0; i < actualSegmentsNum; i++)
    {
        segmentPtr = frameData.getData()+i*segmentSize;
        segments.push_back(segmentPtr);
    }
    
    {
        for (int i = 0; i < actualSegmentsNum; i++)
            segmentsIdx.push_back(i);
        
        std::random_shuffle(segmentsIdx.begin(), segmentsIdx.end());
        
        FrameBuffer::Slot * slot = new FrameBuffer::Slot(slotSize);
        
        EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
        slot->markNew(0, false);
        slot->markAssembling(actualSegmentsNum, segmentSize);
        
        for (int i = 0; i < actualSegmentsNum; i++)
        {
            int segmentIdx = segmentsIdx[i];
            
            unsigned int size = (segmentIdx == actualSegmentsNum-1)?lastSegmentSize:segmentSize;
            
            FrameBuffer::Slot::State state = slot->appendSegment(segmentIdx, size, segments[segmentIdx]);
            
            if (i == actualSegmentsNum-1)
                EXPECT_EQ(FrameBuffer::Slot::StateReady, state);
            else
                EXPECT_EQ(FrameBuffer::Slot::StateAssembling, state);
        }
        
        // check frame consistency
        shared_ptr<webrtc::EncodedImage> frame = slot->getFrame();
        
        EXPECT_NE(nullptr, frame.get());
        NdnRtcObjectTestHelper::checkFrames(encodedFrame_, frame.get());
        
        delete slot;
    }
}

TEST_F(FrameSlotTester, WrongAssemble)
{
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int slotSize = 2*encodedFrame_->_length; // big enough to store encoded frame?
    unsigned int segmentsNum = 3;
    unsigned int segmentSize = encodedFrame_->_length / segmentsNum;
    
    vector<unsigned char*> segments;
    unsigned int lastSegmentSize = frameData.getLength() - segmentsNum*segmentSize;
    
    // slice data into segments
    unsigned int actualSegmentsNum = (lastSegmentSize)?segmentsNum+1:segmentsNum;
    unsigned char *segmentPtr;
    
    for (int i = 0; i < actualSegmentsNum; i++)
    {
        segmentPtr = frameData.getData()+i*segmentSize;
        segments.push_back(segmentPtr);
    }
    
    {
        FrameBuffer::Slot * slot = new FrameBuffer::Slot(slotSize);
        
        EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
        slot->markNew(0, false);
        slot->markAssembling(actualSegmentsNum, segmentSize);
        
        for (int i = 0; i < actualSegmentsNum; i++)
        {
            unsigned int size = (i == actualSegmentsNum-1)?lastSegmentSize:segmentSize;
            
            // segment number in next call is wrong - so that frame segment will be written in a wrong places
            int incorrectSegmentIdx = actualSegmentsNum-i;
            FrameBuffer::Slot::State state = slot->appendSegment(incorrectSegmentIdx, size, segments[i]);
            
            if (i == actualSegmentsNum-1)
                EXPECT_EQ(FrameBuffer::Slot::StateReady, state);
            else
                EXPECT_EQ(FrameBuffer::Slot::StateAssembling, state);
        }
        
        // check frame consistency
        shared_ptr<webrtc::EncodedImage> frame = slot->getFrame();
        
        EXPECT_EQ(nullptr, frame.get());
        
        delete slot;
    }
}

TEST_F(FrameSlotTester, OverloadAssemble)
{
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int slotSize = encodedFrame_->_length/10; // slot size left intentionally small - slot should manage enlarging buffer in order to keep upcoming data
    unsigned int segmentsNum = 3;
    unsigned int segmentSize = encodedFrame_->_length / segmentsNum;
    
    vector<unsigned char*> segments;
    unsigned int lastSegmentSize = frameData.getLength() - segmentsNum*segmentSize;
    
    // slice data into segments
    unsigned int actualSegmentsNum = (lastSegmentSize)?segmentsNum+1:segmentsNum;
    unsigned char *segmentPtr;
    
    for (int i = 0; i < actualSegmentsNum; i++)
    {
        segmentPtr = frameData.getData()+i*segmentSize;
        segments.push_back(segmentPtr);
    }
    
    {
        FrameBuffer::Slot * slot = new FrameBuffer::Slot(slotSize);
        
        EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
        slot->markNew(0, false);
        slot->markAssembling(actualSegmentsNum, segmentSize);
        
        for (int i = 0; i < actualSegmentsNum; i++)
        {
            unsigned int size = (i == actualSegmentsNum-1)?lastSegmentSize:segmentSize;
            
            FrameBuffer::Slot::State state = slot->appendSegment(i, size, segments[i]);
            
            if (i == actualSegmentsNum-1)
                EXPECT_EQ(FrameBuffer::Slot::StateReady, state);
            else
                EXPECT_EQ(FrameBuffer::Slot::StateAssembling, state);
        }
        
        // check frame consistency
        shared_ptr<webrtc::EncodedImage> frame = slot->getFrame();
        
        EXPECT_NE(nullptr, frame.get());
        NdnRtcObjectTestHelper::checkFrames(encodedFrame_, frame.get());
        
        delete slot;
    }
}
#endif
//********************************************************************************
// FrameBuffer tests
class FrameBufferTester : public NdnRtcObjectTestHelper
{
    public :
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        encodedFrame_ = NdnRtcObjectTestHelper::loadEncodedFrame();
        bookingCs_ = webrtc::CriticalSectionWrapper::CreateCriticalSection();
        receivedEventsStack_.clear();
        stopWaiting_ = false;
        
        ASSERT_NE((webrtc::EncodedImage*)NULL, encodedFrame_);
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        if (encodedFrame_)
        {
            delete encodedFrame_->_buffer;
            delete encodedFrame_;
        }
        
        if (bookingCs_)
            delete bookingCs_;
    }
    
    virtual void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
        finishedWaiting_ = false;
        fullyBooked_ = false;
        bookingNo_ = 0;
        waitingCycles_ = 0;
        stopWaiting_ = false;
        receivedEventsStack_.clear();
        memset(&receivedEvent_, 0, sizeof(receivedEvent_));
    }
    
    static Name getPrefixForBookNo(unsigned int bookNo)
    {
        shared_ptr<string> prefix = NdnRtcNamespace::getStreamPath(DefaultParams.ndnHub,
                                                                   DefaultParams.producerId,
                                                                   DefaultParams.streamName);
        shared_ptr<string> full = NdnRtcNamespace::buildPath(false,
                                                             prefix.get(),
                                                             &NdnRtcNamespace::NameComponentStreamFrames,
                                                             &NdnRtcNamespace::NameComponentStreamFramesDelta,
                                                             nullptr);
        
        std::ostringstream oss;
        oss << *full << "/" << bookNo;
        
        return Name(oss.str());
    }
    
    static Data getDataSegment(const Name &prefix, webrtc::EncodedImage &image,
                        int segNo, int segSize)
    {
        int dataSegmentSize = segSize;
        unsigned char *dataSegment = nullptr;
        PacketData::PacketMetadata meta;
        
        meta.packetRate_ = 30.;
        meta.sequencePacketNumber_ = NdnRtcNamespace::getPacketNumber(prefix);
        
        NdnFrameData frameData(image, meta);
        int totalSeg = ceil((double)frameData.getLength()/(double)segSize);
        
        if (frameData.getLength() < segSize)
            dataSegmentSize = frameData.getLength();
        else
            // adjust for last segment
            if (segNo == totalSeg-1)
            {
                dataSegmentSize = frameData.getLength() - (totalSeg-1)*segSize;
            }
        
        dataSegment = frameData.getData()+segSize*segNo;
        
        Name dataPrefix(prefix);
        dataPrefix.appendSegment(segNo);
        dataPrefix.appendFinalSegment(totalSeg-1);
        
        Data data(dataPrefix);
        data.setContent(dataSegment, dataSegmentSize);
        
        return data;
    }
    
protected:
    bool stopWaiting_, finishedWaiting_, fullyBooked_;
    unsigned int waitingCycles_;
    webrtc::EncodedImage *encodedFrame_;
    
    // test thread functions
    unsigned int bookingNo_;
    int expectedBufferEvents_;
    FrameBuffer::Event receivedEvent_;
    FrameBuffer *buffer_;
    vector<FrameBuffer::Event> receivedEventsStack_;
    
    static bool waitBufferEvent(void *obj) { return ((FrameBufferTester*)obj)->waitBuffer(); }
    bool waitBuffer()
    {
        if (!stopWaiting_)
        {
            waitingCycles_++;
            receivedEvent_ = buffer_->waitForEvents(expectedBufferEvents_);
            receivedEventsStack_.push_back(receivedEvent_);
            finishedWaiting_ = true;
            stopWaiting_ = (receivedEvent_.type_ == FrameBuffer::Event::Error);
        }
        return true;
    }
    
    static bool waitBufferEventAndBookSlot(void *obj) { return ((FrameBufferTester*)obj)->waitBufferAndBook(); }
    bool waitBufferAndBook()
    {
        if (!stopWaiting_)
        {
            receivedEvent_ = buffer_->waitForEvents(expectedBufferEvents_);
            finishedWaiting_ = true;
            
            stopWaiting_ = (receivedEvent_.type_ == FrameBuffer::Event::Error);
            
            if (!stopWaiting_)
            {
                BookingId bId;
                Name prefix = getPrefixForBookNo(bookingNo_);
                EXPECT_EQ(FrameBuffer::CallResultNew, buffer_->bookSlot(prefix, bId));

                bookingNo_ = bId+1;
                
                if (buffer_->getStat(FrameBuffer::Slot::StateFree) == 0)
                {
                    Name newPrefix = getPrefixForBookNo(bId+1);
                    EXPECT_EQ(FrameBuffer::CallResultFull, buffer_->bookSlot(newPrefix, bId));
                    fullyBooked_ = true;
                }
            }
        }
        
        return true; //!fullyBooked_;
    }
    
    // booker thread mimics ndn-rtc real-app thread which pipelines interests for the segments and frames
    webrtc::CriticalSectionWrapper *bookingCs_;
    vector<int> bookedFrames_;
    map<int, std::set<Name>> interestsPerFrame_;
    unsigned int lastBooked_, bookingLimit_, bookerCycleCount_;
    int bookerWaitingEvents_;
    bool bookerGotEvent_, bookerFilledBuffer_;
    vector<FrameBuffer::Event> bookerEvents_;
    
    static bool processBookingThread(void *obj)
    { return ((FrameBufferTester*)obj)->processBooking(); }
    bool processBooking()
    {
        if (stopWaiting_)
            return false;
        
        bookerCycleCount_++;
        FrameBuffer::Event ev = buffer_->waitForEvents(bookerWaitingEvents_);
        bookerGotEvent_ = true;
        
        bookerEvents_.push_back(ev);
        
        FrameBuffer::CallResult res;
        
        switch (ev.type_) {
            case FrameBuffer::Event::FreeSlot:
            {
                LOG_TRACE("free slot. booking %d", lastBooked_);
                bookerFilledBuffer_ = false;

                BookingId bId;
                Name prefix = getPrefixForBookNo(lastBooked_);
                
                res = buffer_->bookSlot(prefix, bId);
                lastBooked_ = bId;
                
                LOG_TRACE("issue %s", prefix.toUri().c_str());
                bookingCs_->Enter();
                interestsPerFrame_[lastBooked_++].insert(prefix);
                bookingCs_->Leave();
            }
                break;
            case FrameBuffer::Event::Timeout:
            {
                // in real app, re-issue interest here (if needed): "new interest(ev.frameNo_, ev.segmentNo_)"
            }
                break;
            case FrameBuffer::Event::FirstSegment:
            {
                // in real app, interest pipelining should be established here
                // should check ev.slot_->totalSegmentsNumber() and ev.segmentNo_
                bookingCs_->Enter();
                
                for (int i = 0; i < ev.slot_->totalSegmentsNumber(); i++)
                {
                    if (i == ev.segmentNo_)
                        continue;
                    
                    Name segmentPrefix = getPrefixForBookNo(ev.packetNo_);
                    segmentPrefix.appendSegment(i);
                    interestsPerFrame_[ev.packetNo_].insert(segmentPrefix);
                }
                bookingCs_->Leave();
            }
                break;
            case FrameBuffer::Event::Error:
            {
                stopWaiting_ = true;
            }
                break;
            default:
                break;
        }
        
        bookerFilledBuffer_ = (res == FrameBuffer::CallResultFull);
        
        return true;
    }
    void setupBooker()
    {
        bookerWaitingEvents_ = FrameBuffer::Event::FreeSlot| // for booking slots
        FrameBuffer::Event::FirstSegment| // for pipelining interests
        FrameBuffer::Event::Timeout; // interests re-issuing
        bookerCycleCount_ = 0;
        bookedFrames_.clear();
        lastBooked_ = 0;
        bookerGotEvent_ = false;
        bookerFilledBuffer_ = false;
        bookerEvents_.clear();
        bookingLimit_ = 15; // deafult
    }
    
    unsigned int providerCycleCount_;
    int providerWaitingEvents_;
    bool providerGotEvent_;
    static bool processProviderThread(void *obj) { return ((FrameBufferTester*)obj)->processProvider(); }
    bool processProvider()
    {
        if (stopWaiting_)
            return false;
        
        providerCycleCount_++;
        
        FrameBuffer::Event ev = buffer_->waitForEvents(providerWaitingEvents_);
        providerGotEvent_ = true;
        
        
        FrameBuffer::CallResult res;
        
        switch (ev.type_) {
            case FrameBuffer::Event::Ready:
            {
                LOG_TRACE("%d frame %d is ready. unpacking",
                          providerCycleCount_, ev.packetNo_);
                
                buffer_->lockSlot(ev.slot_->getFrameNumber());
                
                shared_ptr<webrtc::EncodedImage> frame = ev.slot_->getFrame();
                
                EXPECT_NE(nullptr, frame.get());
                
                if (frame.get() != nullptr)
                    NdnRtcObjectTestHelper::checkFrames(encodedFrame_, frame.get());
                
                //                WAIT(rand()%50);
                buffer_->unlockSlot(ev.slot_->getFrameNumber());
                buffer_->markSlotFree(ev.slot_->getFrameNumber());
                LOG_TRACE("frame checked");
            }
                break;
            case FrameBuffer::Event::Error:
                stopWaiting_ = true;
                break;
            default:
                LOG_TRACE("got unexpected event %d", ev.type_);
                break;
                
        }
        
        return true;
    }
    void setupProvider()
    {
        providerCycleCount_ = 0;
        providerGotEvent_ = false;
        providerWaitingEvents_ = FrameBuffer::Event::Ready;
    }
    
    // playout thread - sets up playout buffer based on current frame buffer
    // asks playout buffer for encoded frames with frameRate frequency
    bool playoutShouldStop_, playoutStopped_;
    unsigned int frameRate_, playoutCycleCount_, playoutLoss_, playoutProcessed_;
    long playoutSleepIntervalUSec_; // sleep interval in microseconds for playout iteration
    PlayoutBuffer *playoutBuffer_;
    static bool processPlayoutThread(void *obj) { return ((FrameBufferTester*)obj)->processPlayout(); }
    bool processPlayout()
    {
        LOG_TRACE("playout cycle %d", playoutCycleCount_++);
        if (!playoutShouldStop_)
        {
            int64_t processingDelay = rand()%50;
            
            FrameBuffer::Slot *slot = playoutBuffer_->acquireNextSlot();
            
            if (slot)
            {
                shared_ptr<webrtc::EncodedImage> encodedFrame = slot->getFrame();
                
                // perform decoding
                LOG_TRACE("process frame %d for %ld", playoutBuffer_->framePointer(), processingDelay*1000);
                NdnRtcObjectTestHelper::checkFrames(encodedFrame_, encodedFrame.get());
                
                usleep(processingDelay*1000);
                playoutProcessed_++;
            }
            else
            {
                playoutLoss_++;
                LOG_DBG("couldn't get frame with number %d", playoutBuffer_->framePointer());
            }
            
            playoutBuffer_->releaseAcquiredSlot();
            
            // sleep if needed
            if (playoutSleepIntervalUSec_-processingDelay*1000 > 0)
            {
                usleep(playoutSleepIntervalUSec_-processingDelay*1000);
            }
        }
        else
        {
            LOG_TRACE("playout stopped");
            playoutStopped_ = true;
        }
        
        return !playoutStopped_;
    }
    void setupPlayout()
    {
        playoutShouldStop_ = false;
        playoutStopped_ = false;
        playoutCycleCount_ = 0;
        playoutLoss_ = 0;
        frameRate_ = 30;
        playoutBuffer_ = new PlayoutBuffer();
        playoutProcessed_ = 0;
        playoutSleepIntervalUSec_ = 1000000/frameRate_;
        
        playoutBuffer_->init(buffer_, frameRate_);
    }
};
#if 0
TEST_F(FrameBufferTester, BufferCreateDelete)
{
    FrameBuffer *buffer = new FrameBuffer();
    
    delete buffer;
}

TEST_F(FrameBufferTester, BufferInit)
{
    FrameBuffer *buffer = new FrameBuffer();
    
    unsigned int buffSize = 60;
    unsigned int slotSize = 8000;
    unsigned int segmentSize = 1054;
    
    EXPECT_EQ(0, buffer->init(buffSize, slotSize, segmentSize));
    EXPECT_EQ(60, buffer->getBufferSize());
    EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateFree));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateAssembling));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateLocked));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateReady));
    
    delete buffer;
}

TEST_F(FrameBufferTester, BookSlot)
{
    { // simple book
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        BookingId bId;
        Name prefix = getPrefixForBookNo(32);
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        
        BookingId bId2 = -1;
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(prefix, bId2));
        EXPECT_NE(bId, bId2);
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        delete buffer;
    }
    
    { // test full buffer
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer->init(buffSize, slotSize, segmentSize);
        for (int i = 0; i < buffSize; i++)
        {
            BookingId bId;
            Name prefix = getPrefixForBookNo(i);
            EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        }
        
        BookingId bId;
        Name prefix = getPrefixForBookNo(buffSize);
        EXPECT_EQ(FrameBuffer::CallResultFull, buffer->bookSlot(prefix, bId));
        
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        delete buffer;
    }
    
    { // test book twice
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        BookingId bId;
        Name prefix = getPrefixForBookNo(23);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(prefix, bId));
        
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, BookSlots)
{
    { // booking with prefixes that have segment numbers should resolve into
      // "parent" segment with frame number only
        
        PacketNumber frameNo = 10;
        Name prefix = getPrefixForBookNo(frameNo);
        prefix.appendSegment(0);
        
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        BookingId bookingId;
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bookingId));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        // second try should result in "booked" result
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(prefix, bookingId));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        for (int i = 1; i < 50; i++)
        {
            Name segmentPrefix = getPrefixForBookNo(frameNo);
            segmentPrefix.appendSegment(1);
            
            EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(segmentPrefix, bookingId));
            EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
            EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        }
    }
}
#endif
TEST_F(FrameBufferTester, BookSlots2)
{
    {
        BookingId bId1;
        Name base1("/ndn/ucla.edu/apps/ndnrtc/user/testuser3/streams/video0/vp8/frames/delta/54");
        Name prefix1("/ndn/ucla.edu/apps/ndnrtc/user/testuser3/streams/video0/vp8/frames/delta/54/%00");
        Name prefix1Data("/ndn/ucla.edu/apps/ndnrtc/user/testuser3/streams/video0/vp8/frames/delta/54/%00%01/%C1.M.FINAL%00%06");
        
        BookingId bId2;
        Name base2("/ndn/ucla.edu/apps/ndnrtc/user/testuser3/streams/video0/vp8/frames/delta/55");
        Name prefix2("/ndn/ucla.edu/apps/ndnrtc/user/testuser3/streams/video0/vp8/frames/delta/55/%00");
        Name prefix2Data("/ndn/ucla.edu/apps/ndnrtc/user/testuser3/streams/video0/vp8/frames/delta/55/%00%05/%C1.M.FINAL%00%06");

        
        FrameBuffer *buffer = new FrameBuffer();
        unsigned int buffSize = 10;
        unsigned int slotSize = 8000;
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        PacketData::PacketMetadata meta;
        NdnFrameData frameData(*frame);
        unsigned int segmentSize = 100; //frame->getLength()/segmentNum;
        unsigned int segmentNum = ceil((double)frameData.getLength()/(double)segmentSize);
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix1, bId1));
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix2, bId2));
        EXPECT_NE(bId1, bId2);
        
        for (int i = 0; i < segmentNum; i++)
        {
            FrameBuffer::CallResult res = buffer->appendSegment(getDataSegment(base1, *frame, i, segmentSize));
            
            if (i == segmentNum-1)
                EXPECT_EQ(FrameBuffer::CallResultReady, res);
            else
                EXPECT_EQ(FrameBuffer::CallResultAssembling, res);
        }
        
        for (int i = 0; i < segmentNum; i++)
        {
            FrameBuffer::CallResult res = buffer->appendSegment(getDataSegment(base2, *frame, i, segmentSize));
            
            if (i == segmentNum-1)
                EXPECT_EQ(FrameBuffer::CallResultReady, res);
            else
                EXPECT_EQ(FrameBuffer::CallResultAssembling, res);
        }
        
    }
}
#if 0
TEST_F(FrameBufferTester, FreeSlot)
{
    { // free non-existent
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        BookingId bId;
        Name prefix = getPrefixForBookNo(10);
        
        buffer->bookSlot(prefix, bId);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        buffer->markSlotFree(prefix);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateFree));

        delete buffer;
    }
    
    { // free non-existent
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        BookingId bId;
        Name prefix = getPrefixForBookNo(10);
        
        buffer->bookSlot(prefix, bId);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        // couldn't free slot as it has no frame number yet
        buffer->markSlotFree(10);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        // append with data
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        buffer->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize));
        
        // now we can free slot with this frame number
        buffer->markSlotFree(10);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, BookAndFreeSimple)
{
    { // simple book
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        PacketNumber frameNo = 32;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));

        buffer->markSlotFree(prefix);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        delete buffer;
    }
    
    { // simple book
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        PacketNumber frameNo = 32;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        shared_ptr<string> p = NdnRtcNamespace::getStreamPath(DefaultParams.ndnHub,
                                                                   DefaultParams.producerId,
                                                                   DefaultParams.streamName);
        shared_ptr<string> full = NdnRtcNamespace::buildPath(false,
                                                             p.get(),
                                                             &NdnRtcNamespace::NameComponentStreamFrames,
                                                             &NdnRtcNamespace::NameComponentStreamFramesDelta,
                                                             nullptr);

        
        // shouldn't free
        buffer->markSlotFree(Name(*p));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        // shouldn't free
        buffer->markSlotFree(Name(*full));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));

        for (int i = 0; i < 2*frameNo; i++)
        {
            if (i == frameNo)
                continue;
            
            // shouldn't free
            buffer->markSlotFree(getPrefixForBookNo(i));
            EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
            EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        }
        for (int i = 0; i < 2*frameNo; i++)
        {
            if (i == frameNo)
                continue;
            
            Name pr(getPrefixForBookNo(i));
            pr.appendSegment(NdnRtcObjectTestHelper::randomInt(0,10));

            // shouldn't free
            buffer->markSlotFree(getPrefixForBookNo(i));
            EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
            EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        }
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, LockUnlockFree)
{
    { // try to free locked slot
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        PacketNumber frameNo = 32;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        buffer->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize));
        
        buffer->lockSlot(frameNo);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        // shouldn't be freed
        buffer->markSlotFree(frameNo);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        delete buffer;
    }

    { // standard sequence
        flushFlags();
        
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        PacketNumber frameNo = 32;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        buffer->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateReady));
        
        buffer->lockSlot(frameNo);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        buffer->unlockSlot(frameNo);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateReady));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        buffer->markSlotFree(frameNo);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        
        delete buffer;
    }
    { // access non-existent frames
        flushFlags();
        
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        PacketNumber frameNo = 32;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer->init(buffSize, slotSize, segmentSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        
        buffer->lockSlot(frameNo+1);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        buffer->unlockSlot(frameNo+2);
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        buffer->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize));
        buffer->markSlotFree(frameNo);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(prefix, bId));
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, FreeSlotEvent)
{
    { // check free event
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 1000);
        
        EXPECT_EQ(FrameBuffer::Event::FreeSlot,receivedEvent_.type_);
        
        // don't know for sure
        EXPECT_TRUE(buffSize+1 == waitingCycles_ || buffSize == waitingCycles_);
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, BookOnFreeEvent)
{
    {
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEventAndBookSlot, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        // wait until fully booked
        EXPECT_TRUE_WAIT(fullyBooked_, 1000);
        
        finishedWaiting_ = false;
        
        // now mark slot as free so other thread can recevie event
        buffer_->markSlotFree(getPrefixForBookNo(7));
        
        EXPECT_TRUE_WAIT(bookingNo_ == buffSize+1, 1000);
        EXPECT_EQ(FrameBuffer::Event::FreeSlot, receivedEvent_.type_);
        
        EXPECT_EQ(buffSize, buffer_->getStat(FrameBuffer::Slot::StateNew));
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, MarkFreeTwice)
{
    {
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        unsigned int segmentSize = 1054;
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEventAndBookSlot, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        // wait until fully booked
        EXPECT_TRUE_WAIT(fullyBooked_, 1000);
        
        finishedWaiting_ = false;
        
        // now mark slot as free so other thread can recevie event
        buffer_->markSlotFree(getPrefixForBookNo(7));
        
        EXPECT_TRUE_WAIT(bookingNo_ == buffSize+1, 1000);
        EXPECT_EQ(FrameBuffer::Event::FreeSlot,receivedEvent_.type_);
        
        memset(&receivedEvent_, 0, sizeof(receivedEvent_));
        flushFlags();
        
        // waiting thread should not receive event
        buffer_->markSlotFree(getPrefixForBookNo(7));
        
        EXPECT_TRUE_WAIT(!finishedWaiting_, 1000);
        EXPECT_EQ(0,receivedEvent_.type_);
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, TestEventFirstSegment)
{
    {
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentSize = 100;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::FirstSegment;
        buffer_->bookSlot(prefix, bId);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize)));
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::FirstSegment, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
        EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, TestEventReady)
{
    { // 1 segment
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentsNum = 1;
        unsigned int segmentSize = 1000;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::Ready | FrameBuffer::Event::FirstSegment;
        buffer_->bookSlot(prefix, bId);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        WAIT(100); // wait for wating thread to be blocked after first freeframe event
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        EXPECT_EQ(FrameBuffer::CallResultReady, buffer_->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize)));
        
        // should get 2 events - ready and first segment
        EXPECT_TRUE_WAIT(2 == receivedEventsStack_.size(), 1000);
        ASSERT_EQ(2, receivedEventsStack_.size());
        EXPECT_EQ(FrameBuffer::Event::FirstSegment, receivedEventsStack_[0].type_);
        EXPECT_EQ(FrameBuffer::Event::Ready, receivedEventsStack_[1].type_);
        EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
        EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
        
        EXPECT_EQ(buffSize-1, buffer_->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer_->getStat(FrameBuffer::Slot::StateReady));
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }

    { // several segments
        flushFlags();
        
        buffer_ = new FrameBuffer();
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        NdnFrameData data(*frame);
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentSize = 100;
        unsigned int segmentsNum = ceil((double)data.getLength()/(double)segmentSize);
        unsigned int lastSegmentNo = segmentsNum-1;

        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::Ready;
        buffer_->bookSlot(prefix, bId);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        WAIT(100); // wait for watiing thread to be blocked after first freeframe event

        for (int i = 0; i < segmentsNum; i++)
        {
            FrameBuffer::CallResult res = buffer_->appendSegment((getDataSegment(prefix, *frame, i, segmentSize)));
            
            if (i < segmentsNum-1)
                EXPECT_EQ(FrameBuffer::CallResultAssembling, res);
            else
                EXPECT_EQ(FrameBuffer::CallResultReady, res);
                
            EXPECT_FALSE(finishedWaiting_);
        }
        
        EXPECT_FALSE(finishedWaiting_);
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::Ready, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
        EXPECT_EQ(lastSegmentNo, receivedEvent_.segmentNo_);
        
        EXPECT_EQ(buffSize-1, buffer_->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer_->getStat(FrameBuffer::Slot::StateReady));
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, TestEventTimeout)
{
    {
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentsNum = 1;
        unsigned int segmentSize = 1054;
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer_->init(buffSize, slotSize, segmentSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::Timeout;
        buffer_->bookSlot(prefix, bId);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        prefix.appendSegment(segmentNo);
        buffer_->notifySegmentTimeout(prefix);
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::Timeout, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
        EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, TestRandomBufferEvents)
{
    {
        buffer_ = new FrameBuffer();
        
        webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
        NdnFrameData data(*frame);
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentSize = 100;
        unsigned int segmentsNum = ceil((double)data.getLength()/(double)segmentSize);
        BookingId bId;
        Name prefix = getPrefixForBookNo(frameNo);
        
        buffer_->init(buffSize, slotSize, segmentSize);

        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        buffer_->bookSlot(prefix, bId);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        WAIT(100); // wait for watiing thread to be blocked after first freeframe event
        
        { // timeout
            flushFlags();
            
            Name prefixTo(prefix);
            prefixTo.appendSegment(segmentNo);
            
            buffer_->notifySegmentTimeout(prefixTo);
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::Timeout, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
            EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
        }
        
        { // first segment
            flushFlags();
            
            EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(getDataSegment(prefix, *frame, 0, segmentSize)));
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::FirstSegment, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
            EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
            
            EXPECT_EQ(1, buffer_->getStat(FrameBuffer::Slot::StateAssembling));
        }
        
        { // ready
            flushFlags();

            for (int i = 1; i < segmentsNum; i++)
            {
                FrameBuffer::CallResult res = buffer_->appendSegment(getDataSegment(prefix, *frame, i, segmentSize));
                if (i < segmentsNum -1)
                    EXPECT_EQ(FrameBuffer::CallResultAssembling, res);
                else
                    EXPECT_EQ(FrameBuffer::CallResultReady, res);
                
                EXPECT_FALSE(finishedWaiting_);
            }
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::Ready, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.packetNo_);
            EXPECT_EQ(segmentsNum-1, receivedEvent_.segmentNo_);
            
            EXPECT_EQ(1, buffer_->getStat(FrameBuffer::Slot::StateReady));
        }
        
        waitingThread.SetNotAlive();
        buffer_->release();
        waitingThread.Stop();
        
        delete buffer_;
    }
}

TEST_F(FrameBufferTester, TestFull)
{
    // there are going to be 3 threads:
    // - booking thread (books slots in a buffer, listens to events: FirstSegment, FreeSlot, Timeout)
    // - assembling thread - appends segments (current main thread)
    // - frame provider thread (supplies full assembled frames by locking them for random small
    //      amount of time, listens to events: Ready)
    // buffer will be smaller than the ammount of frames needed to be delivered
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int payloadSize = frameData.getLength();
    unsigned int framesNumber = 500;
    unsigned int bufferSize = 5;
    unsigned int slotSize = 4000;
    unsigned int fullSegmentsNumber = 7;
    unsigned int segmentSize = payloadSize/fullSegmentsNumber;
    unsigned int totalSegmentNumber = (payloadSize - segmentSize*fullSegmentsNumber !=0)?fullSegmentsNumber+1:fullSegmentsNumber;
    map<unsigned int, unsigned char*> frameSegments;
    map<unsigned int, set<unsigned int>> remainingSegments;
    
    buffer_ = new FrameBuffer();
    buffer_->setLogger(NdnLogger::sharedInstance());
    buffer_->init(bufferSize, slotSize, segmentSize);
    
    for (unsigned int i = 0; i < totalSegmentNumber; i++)
        frameSegments[i] = frameData.getData()+i*segmentSize;
    
    for (unsigned int i = 0; i < framesNumber; i++)
    {
        vector<unsigned int> currentFrameSegments;
        for (int j = 0; j < totalSegmentNumber; j++)
            currentFrameSegments.push_back(j);
        
        random_shuffle(currentFrameSegments.begin(), currentFrameSegments.end());
        
        for (int j = 0; j < totalSegmentNumber; j++)
            remainingSegments[i].insert(currentFrameSegments[j]);
    }
    
    webrtc::ThreadWrapper &bookingThread(*webrtc::ThreadWrapper::CreateThread(processBookingThread, this));
    webrtc::ThreadWrapper &providerThread(*webrtc::ThreadWrapper::CreateThread(processProviderThread, this));
    
    setupBooker();
    setupProvider();
    
    unsigned int tid;
    bookingThread.Start(tid);
    providerThread.Start(tid);
    
    // append data unless we have interests to reply
    bool stop = false;
    unsigned int sentFrames = 0;
    
    while (!stop)
    {
        bool fetchedInterest;
        // check number of sent interests per frame
        unsigned int frameNo;
        std::set<Name> pendingInterests;
        bool gotPendingInterests = false;
        
        bookingCs_->Enter();
        for (map<int, set<Name>>::iterator it = interestsPerFrame_.begin();
             it != interestsPerFrame_.end(); ++it)
        {
            frameNo = it->first;
            pendingInterests = it->second;
            interestsPerFrame_.erase(it);
            gotPendingInterests = true;
            
            break;
        }
        bookingCs_->Leave();
        
        // append segment for each interest in random order
        if (gotPendingInterests && frameNo < framesNumber)
        {
            LOG_TRACE("got %d interests for frame %d", pendingInterests.size(), frameNo);
            for (set<Name>::iterator it = pendingInterests.begin();
                 it != pendingInterests.end(); ++it)
            {
                unsigned int segmentNo = NdnRtcNamespace::getSegmentNumber(*it);
                
                if (segmentNo == -1)
                {
                    segmentNo = *remainingSegments[frameNo].begin();
                    bookingCs_->Enter();
                    bookingCs_->Leave();
                }
                
                unsigned int currentSegmentSize = (segmentNo == totalSegmentNumber-1) ? payloadSize - segmentSize*fullSegmentsNumber : segmentSize;
                
                LOG_TRACE("appending frameno %d segno %d", frameNo, segmentNo);
                buffer_->appendSegment(getDataSegment(*it, *encodedFrame_, segmentNo, segmentSize));
                
                remainingSegments[frameNo].erase(segmentNo);
                if (remainingSegments[frameNo].size() == 0)
                    remainingSegments.erase(frameNo);
                
                bookingCs_->Enter();
                interestsPerFrame_[frameNo].erase(*it);
                if (interestsPerFrame_[frameNo].size() == 0)
                {
                    interestsPerFrame_.erase(frameNo);
                }
                bookingCs_->Leave();
            }
        }
        
        stop = (remainingSegments.size() == 0);
        if (stop)
            LOG_TRACE("no more remaining segments. stopping");
    }
    
    EXPECT_TRUE_WAIT((providerCycleCount_ == framesNumber || providerCycleCount_ == framesNumber+1), 5000);
    cout << "processed " << providerCycleCount_ << " published " << framesNumber << endl;
    
    bookingThread.SetNotAlive();
    providerThread.SetNotAlive();
    
    buffer_->release();
    
    bookingThread.Stop();
    providerThread.Stop();
    
    delete buffer_;
}

TEST_F(FrameBufferTester, TestFullWithPlayoutBuffer)
{
    // there are going to be 3 threads:
    // - booking thread (books slots in a buffer, listens to events: FirstSegment, FreeSlot, Timeout)
    // - assembling thread - appends segments (current main thread)
    // - frame provider thread (supplies full assembled frames by locking them for random small
    //      amount of time, listens to events: Ready)
    // buffer will be smaller than the ammount of frames needed to be delivered
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int payloadSize = frameData.getLength();
    unsigned int framesNumber = 50;
    unsigned int bufferSize = 5;
    unsigned int slotSize = 4000;
    unsigned int fullSegmentsNumber = 7;
    unsigned int segmentSize = payloadSize/fullSegmentsNumber;
    unsigned int totalSegmentNumber = (payloadSize - segmentSize*fullSegmentsNumber !=0)?fullSegmentsNumber+1:fullSegmentsNumber;
    map<unsigned int, unsigned char*> frameSegments;
    map<unsigned int, set<unsigned int>> remainingSegments;
    
    buffer_ = new FrameBuffer();
    buffer_->setLogger(NdnLogger::sharedInstance());
    buffer_->init(bufferSize, slotSize, segmentSize);
    
    for (unsigned int i = 0; i < totalSegmentNumber; i++)
        frameSegments[i] = frameData.getData()+i*segmentSize;
    
    for (unsigned int i = 0; i < framesNumber; i++)
    {
        vector<unsigned int> currentFrameSegments;
        for (int j = 0; j < totalSegmentNumber; j++)
            currentFrameSegments.push_back(j);
        
        random_shuffle(currentFrameSegments.begin(), currentFrameSegments.end());
        
        for (int j = 0; j < totalSegmentNumber; j++)
            remainingSegments[i].insert(currentFrameSegments[j]);
    }
    
    webrtc::ThreadWrapper &bookingThread(*webrtc::ThreadWrapper::CreateThread(processBookingThread, this));
    webrtc::ThreadWrapper &playoutThread(*webrtc::ThreadWrapper::CreateThread(processPlayoutThread, this));
    
    setupBooker();
    setupPlayout();
    
    unsigned int tid = 1;
    bookingThread.Start(tid);
    playoutThread.Start(tid);
    
    // append data unless we have interests to reply
    bool stop = false;
    unsigned int sentFrames = 0;
    
    while (!stop)
    {
        bool fetchedInterest;
        // check number of sent interests per frame
        unsigned int frameNo;
        std::set<Name> pendingInterests;
        bool gotPendingInterests = false;
        
        bookingCs_->Enter();
        for (map<int, set<Name>>::iterator it = interestsPerFrame_.begin();
             it != interestsPerFrame_.end(); ++it)
        {
            frameNo = it->first;
            pendingInterests = it->second;
            interestsPerFrame_.erase(it);
            gotPendingInterests = true;
            
            break;
        }
        bookingCs_->Leave();
        
        // append segment for each interest in random order
        if (gotPendingInterests && frameNo < framesNumber)
        {
            LOG_TRACE("got %d interests for frame %d", pendingInterests.size(), frameNo);
            for (set<Name>::iterator it = pendingInterests.begin();
                 it != pendingInterests.end(); ++it)
            {
                unsigned int segmentNo = NdnRtcNamespace::getSegmentNumber(*it);
                
                if (segmentNo == -1)
                {
                    segmentNo = *remainingSegments[frameNo].begin();
                    bookingCs_->Enter();
                    bookingCs_->Leave();
                }
                
                unsigned int currentSegmentSize = (segmentNo == totalSegmentNumber-1) ? payloadSize - segmentSize*fullSegmentsNumber : segmentSize;
                
                LOG_TRACE("appending frameno %d segno %d", frameNo, segmentNo);
                
                
                buffer_->appendSegment(getDataSegment(*it, *encodedFrame_, segmentNo, segmentSize));
                
                remainingSegments[frameNo].erase(segmentNo);
                if (remainingSegments[frameNo].size() == 0)
                    remainingSegments.erase(frameNo);
                
                bookingCs_->Enter();
                interestsPerFrame_[frameNo].erase(*it);
                if (interestsPerFrame_[frameNo].size() == 0)
                {
                    interestsPerFrame_.erase(frameNo);
                }
                bookingCs_->Leave();
            }
        }
        
        stop = (remainingSegments.size() == 0);
    }
    
    EXPECT_TRUE_WAIT((playoutProcessed_ == framesNumber), 2000);
    cout << "processed " << playoutProcessed_ << " published " << framesNumber << endl;
    
    playoutShouldStop_ = true;

    bookingThread.SetNotAlive();
    playoutThread.SetNotAlive();
    
    buffer_->release();
    
    bookingThread.Stop();
    playoutThread.Stop();
    
    delete buffer_;
}

//********************************************************************************
// PlayoutBuffer tests
class PlayoutBufferTester : public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
    }
    void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
    }
    
protected:
    
};

TEST_F(PlayoutBufferTester, CreateDelete)
{
    PlayoutBuffer *buffer = new PlayoutBuffer();
    
    delete buffer;
}

TEST_F(PlayoutBufferTester, Init)
{
    FrameBuffer *frameBuffer = new FrameBuffer();
    PlayoutBuffer *playoutBuffer = new PlayoutBuffer();
    
    frameBuffer->init(1, 8000, 1054);
    
    EXPECT_EQ(0, playoutBuffer->init(frameBuffer, 30));
    WAIT(100);
    frameBuffer->release();
    WAIT(100);
    
    delete frameBuffer;
    delete playoutBuffer;
}

TEST_F(PlayoutBufferTester, AcquireFrame)
{
    FrameBuffer *frameBuffer = new FrameBuffer();
    PlayoutBuffer *playoutBuffer = new PlayoutBuffer();
    unsigned int frameNo = 1;
    
    frameBuffer->init(1, 8000, 1054);
    playoutBuffer->init(frameBuffer, 30);
    
    webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
    ASSERT_NE((webrtc::EncodedImage*)NULL, frame);
    
    NdnFrameData frameData(*frame);
    
    FrameBuffer::Slot *slot = playoutBuffer->acquireNextSlot();
    ASSERT_EQ(nullptr, slot);
    
    // append new frame
    BookingId bId;
    Name prefix = FrameBufferTester::getPrefixForBookNo(frameNo);
    frameBuffer->bookSlot(prefix, bId);
    frameBuffer->appendSegment(FrameBufferTester::getDataSegment(prefix, *frame, 0, 1054));
    
    shared_ptr<webrtc::EncodedImage> assembledFrame(nullptr);
    
    unsigned int timeout = 1000;
    while (slot == nullptr && timeout > 0)
    {
        slot = playoutBuffer->acquireNextSlot();
        
        if (slot)
            assembledFrame = slot->getFrame();
        
        WAIT(10);
        timeout -= 10;
    }
    
    ASSERT_NE(nullptr, assembledFrame.get());
    
    NdnRtcObjectTestHelper::checkFrames(frame, assembledFrame.get());
    
    frameBuffer->release();
    WAIT(100);
    
    delete frameBuffer;
    delete playoutBuffer;
}
#endif