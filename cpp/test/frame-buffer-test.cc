//
//  frame-buffer-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//



#include "test-common.h"
#include "frame-buffer.h"
#include "playout-buffer.h"
#include "video-receiver.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));


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
    EXPECT_EQ(-1, slot->getFrameNumber());
    delete slot;
}

TEST_F(FrameSlotTester, TestStates)
{
    FrameBuffer::Slot * slot = new FrameBuffer::Slot(4000);
    
    EXPECT_EQ(FrameBuffer::Slot::StateFree, slot->getState());
    
    slot->markNew(123);
    EXPECT_EQ(FrameBuffer::Slot::StateNew, slot->getState());
    EXPECT_EQ(123, slot->getFrameNumber());
    
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
        slot->markNew(0);
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
        slot->markNew(0);
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
        slot->markNew(0);
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
        slot->markNew(0);
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
            stopWaiting_ = (receivedEvent_.type_ == FrameBuffer::Event::EventTypeError);
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
            
            stopWaiting_ = (receivedEvent_.type_ == FrameBuffer::Event::EventTypeError);
            
            if (!stopWaiting_)
            {
                EXPECT_EQ(FrameBuffer::CallResultNew, buffer_->bookSlot(bookingNo_++));
                
                
                if (bookingNo_ == buffer_->getBufferSize())
                {
                    EXPECT_EQ(FrameBuffer::CallResultFull, buffer_->bookSlot(bookingNo_));
                    fullyBooked_ = true;
                }
            }
        }
        
        return true; //!fullyBooked_;
    }
    
    
    // booker thread mimics ndn-rtc real-app thread which pipelines interests for the segments and frames
    webrtc::CriticalSectionWrapper *bookingCs_;
    vector<int> bookedFrames_;
    map<int, int> interestsPerFrame_;
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
            case FrameBuffer::Event::EventTypeFreeSlot:
                LOG_TRACE("free slot. booking %d", lastBooked_);
                bookerFilledBuffer_ = false;
                res = buffer_->bookSlot(lastBooked_);
                bookingCs_->Enter();
                interestsPerFrame_[lastBooked_++] = 1;
                bookingCs_->Leave();
                break;
            case FrameBuffer::Event::EventTypeTimeout:
                // in real app, re-issue interest here (if needed): "new interest(ev.frameNo_, ev.segmentNo_)"
                break;
            case FrameBuffer::Event::EventTypeFirstSegment:
                // in real app, interest pipelining should be established here
                // should check ev.slot_->totalSegmentsNumber() and ev.segmentNo_
                bookingCs_->Enter();
                interestsPerFrame_[ev.frameNo_] += (ev.slot_->totalSegmentsNumber() - 1);
                bookingCs_->Leave();
                break;
            case FrameBuffer::Event::EventTypeError:
                stopWaiting_ = true;
                break;
            default:
                break;
        }
        
        bookerFilledBuffer_ = (res == FrameBuffer::CallResultFull);
        
        return true;
    }
    void setupBooker()
    {
        bookerWaitingEvents_ = FrameBuffer::Event::EventTypeFreeSlot| // for booking slots
        FrameBuffer::Event::EventTypeFirstSegment| // for pipelining interests
        FrameBuffer::Event::EventTypeTimeout; // interests re-issuing
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
            case FrameBuffer::Event::EventTypeReady:
            {
                LOG_TRACE("%d frame %d is ready. unpacking",providerCycleCount_, ev.frameNo_);
                
                buffer_->lockSlot(ev.frameNo_);
                
                shared_ptr<webrtc::EncodedImage> frame = ev.slot_->getFrame();
                
                EXPECT_NE(nullptr, frame.get());
                
                if (frame.get() != nullptr)
                    NdnRtcObjectTestHelper::checkFrames(encodedFrame_, frame.get());
                
                //                WAIT(rand()%50);
                buffer_->unlockSlot(ev.frameNo_);
                buffer_->markSlotFree(ev.frameNo_);
                LOG_TRACE("frame checked");
            }
                break;
            case FrameBuffer::Event::EventTypeError:
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
        providerWaitingEvents_ = FrameBuffer::Event::EventTypeReady;
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
    
    EXPECT_EQ(0, buffer->init(buffSize, slotSize));
    EXPECT_EQ(60, buffer->getBufferSize());
    EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateFree));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateAssembling));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateLocked));
    EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateReady));
    EXPECT_EQ(0, buffer->getTimeoutsNumber());
    
    delete buffer;
}

TEST_F(FrameBufferTester, BookSlot)
{
    { // simple book
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        delete buffer;
    }
    
    { // test full buffer
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        for (int i = 0; i < buffSize; i++)
        {
            EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(i));
        }
        
        EXPECT_EQ(FrameBuffer::CallResultFull, buffer->bookSlot(buffSize));
        
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        delete buffer;
    }
    
    { // test book twice
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(32));
        
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, FreeSlot)
{
    { // free non-existent
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        
        buffer->bookSlot(10);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        
        buffer->markSlotFree(32);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultFull, buffer->bookSlot(11));
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
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(32));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));

        buffer->markSlotFree(32);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, LockUnlockFree)
{
    { // try to free locked slot
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        buffer->lockSlot(32);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        // shouldn't be freed
        buffer->markSlotFree(32);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(32));
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
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        buffer->lockSlot(32);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        buffer->unlockSlot(32);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        buffer->markSlotFree(32);
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize, buffer->getStat(FrameBuffer::Slot::StateFree));
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        
        delete buffer;
    }
    { // access non-existent frames
        flushFlags();
        
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        
        buffer->lockSlot(33);
        EXPECT_EQ(1, buffer->getStat(FrameBuffer::Slot::StateNew));
        EXPECT_EQ(buffSize-1, buffer->getStat(FrameBuffer::Slot::StateFree));
        EXPECT_EQ(0, buffer->getStat(FrameBuffer::Slot::StateLocked));
        
        buffer->unlockSlot(34);
        
        buffer->markSlotFree(32);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        
        delete buffer;
    }
}

TEST_F(FrameBufferTester, FreeSlotEvent)
{
    { // check free event
        buffer_ = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 1000);
        
        EXPECT_EQ(FrameBuffer::Event::EventTypeFreeSlot,receivedEvent_.type_);
        
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
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEventAndBookSlot, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        // wait until fully booked
        EXPECT_TRUE_WAIT(fullyBooked_, 1000);
        
        finishedWaiting_ = false;
        
        // now mark slot as free so other thread can recevie event
        buffer_->markSlotFree(7);
        
        EXPECT_TRUE_WAIT(bookingNo_ == buffSize+1, 1000);
        EXPECT_EQ(FrameBuffer::Event::EventTypeFreeSlot,receivedEvent_.type_);
        EXPECT_EQ(7, receivedEvent_.frameNo_);
        
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
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEventAndBookSlot, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        // wait until fully booked
        EXPECT_TRUE_WAIT(fullyBooked_, 1000);
        
        finishedWaiting_ = false;
        
        // now mark slot as free so other thread can recevie event
        buffer_->markSlotFree(7);
        
        EXPECT_TRUE_WAIT(bookingNo_ == buffSize+1, 1000);
        EXPECT_EQ(FrameBuffer::Event::EventTypeFreeSlot,receivedEvent_.type_);
        EXPECT_EQ(7, receivedEvent_.frameNo_);
        
        memset(&receivedEvent_, 0, sizeof(receivedEvent_));
        flushFlags();
        
        // waiting thread should not receive event
        buffer_->markSlotFree(7);
        
        EXPECT_TRUE_WAIT(!finishedWaiting_, 1000);
        EXPECT_EQ(0,receivedEvent_.type_);
        EXPECT_EQ(0, receivedEvent_.frameNo_);
        
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
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::EventTypeFirstSegment;
        buffer_->bookSlot(frameNo);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        unsigned char segment[100];
        
        buffer_->markSlotAssembling(frameNo, 2, 100);
        EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(frameNo, segmentNo, 100, segment));
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::EventTypeFirstSegment, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
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
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::EventTypeReady | FrameBuffer::Event::EventTypeFirstSegment;
        buffer_->bookSlot(frameNo);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        WAIT(100); // wait for watiing thread to be blocked after first freeframe event
        
        unsigned char segment[100];
        
        buffer_->markSlotAssembling(frameNo, segmentsNum, 100);
        EXPECT_EQ(FrameBuffer::CallResultReady, buffer_->appendSegment(frameNo, segmentNo, 100, segment));
        
        // should get 2 events - ready and first segment
        EXPECT_TRUE_WAIT(2 == receivedEventsStack_.size(), 1000);
        ASSERT_EQ(2, receivedEventsStack_.size());
        EXPECT_EQ(FrameBuffer::Event::EventTypeFirstSegment, receivedEventsStack_[0].type_);
        EXPECT_EQ(FrameBuffer::Event::EventTypeReady, receivedEventsStack_[1].type_);
        EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
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
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentsNum = 3;
        unsigned int lastSegmentNo = segmentsNum-1;
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::EventTypeReady;
        buffer_->bookSlot(frameNo);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        WAIT(100); // wait for watiing thread to be blocked after first freeframe event
        
        unsigned char segment[100];
        
        buffer_->markSlotAssembling(frameNo, segmentsNum, 100);
        
        for (int i = 0; i < segmentsNum; i++)
        {
            FrameBuffer::CallResult res = buffer_->appendSegment(frameNo, i, 100, segment);
            
            if (i < segmentsNum-1)
                EXPECT_EQ(FrameBuffer::CallResultAssembling, res);
            else
                EXPECT_EQ(FrameBuffer::CallResultReady, res);
                
            EXPECT_FALSE(finishedWaiting_);
        }
        
        EXPECT_FALSE(finishedWaiting_);
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::EventTypeReady, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
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
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::EventTypeTimeout;
        buffer_->bookSlot(frameNo);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        buffer_->notifySegmentTimeout(frameNo, segmentNo);
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::EventTypeTimeout, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
        EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
        
        EXPECT_EQ(1, buffer_->getTimeoutsNumber());
        
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
        
        unsigned int buffSize = 1;
        unsigned int slotSize = 8000;
        unsigned int frameNo = 11;
        unsigned int segmentNo = 0;
        unsigned int segmentsNum = 3;
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        buffer_->bookSlot(frameNo);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        WAIT(100); // wait for watiing thread to be blocked after first freeframe event
        
        { // timeout
            flushFlags();
            buffer_->notifySegmentTimeout(frameNo, segmentNo);
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::EventTypeTimeout, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
            EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
            
            EXPECT_EQ(1, buffer_->getTimeoutsNumber());
        }
        
        { // first segment
            flushFlags();
            unsigned char segment[100];
            
            buffer_->markSlotAssembling(frameNo, segmentsNum, 100);
            EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(frameNo, segmentNo, 100, segment));
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::EventTypeFirstSegment, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
            EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
            
            EXPECT_EQ(1, buffer_->getStat(FrameBuffer::Slot::StateAssembling));
        }
        
        { // ready
            flushFlags();
            unsigned char segment[100];
            
            for (int i = 1; i < segmentsNum; i++)
            {
                FrameBuffer::CallResult res = buffer_->appendSegment(frameNo, i, 100, segment);
                if (i < segmentsNum -1)
                    EXPECT_EQ(FrameBuffer::CallResultAssembling, res);
                else
                    EXPECT_EQ(FrameBuffer::CallResultReady, res);
                
                EXPECT_FALSE(finishedWaiting_);
            }
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::EventTypeReady, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
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
    unsigned int framesNumber = 15;
    unsigned int bufferSize = 5;
    unsigned int slotSize = 4000;
    unsigned int fullSegmentsNumber = 7;
    unsigned int segmentSize = payloadSize/fullSegmentsNumber;
    unsigned int totalSegmentNumber = (payloadSize - segmentSize*fullSegmentsNumber !=0)?fullSegmentsNumber+1:fullSegmentsNumber;
    map<unsigned int, unsigned char*> frameSegments;
    map<unsigned int, vector<unsigned int>> remainingSegments;
    
    buffer_ = new FrameBuffer();
    buffer_->init(bufferSize, slotSize);
    
    for (unsigned int i = 0; i < totalSegmentNumber; i++)
        frameSegments[i] = frameData.getData()+i*segmentSize;
    
    for (unsigned int i = 0; i < framesNumber; i++)
    {
        vector<unsigned int> currentFrameSegments;
        for (int j = 0; j < totalSegmentNumber; j++)
            currentFrameSegments.push_back(j);
        
        random_shuffle(currentFrameSegments.begin(), currentFrameSegments.end());
        remainingSegments[i] = currentFrameSegments;
    }
    
    webrtc::ThreadWrapper &bookingThread(*webrtc::ThreadWrapper::CreateThread(processBookingThread, this));
    //    webrtc::ThreadWrapper &assemblingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
    webrtc::ThreadWrapper &providerThread(*webrtc::ThreadWrapper::CreateThread(processProviderThread, this));
    
    setupBooker();
    setupProvider();
    
    unsigned int tid = 1;
    bookingThread.Start(tid);
    tid++;
    providerThread.Start(tid);
    
    // append data unless we have interests to reply
    bool stop = false;
    unsigned int sentFrames = 0;
    
    while (!stop)
    {
        bool fetchedInterest;
        bookingCs_->Enter();
        // check number of sent interests per frame
        unsigned int frameNo;
        unsigned int interestNumber;
        bool gotPendingInterests = false;
        
        for (map<int, int>::iterator it = interestsPerFrame_.begin(); it != interestsPerFrame_.end(); ++it)
        {
            frameNo = it->first;
            interestNumber = it->second;
            interestsPerFrame_.erase(it);
            gotPendingInterests = true;
            
            break;
        }
        bookingCs_->Leave();
        
        // number of interests should be equal to the number of segments per frame or 1 (when first segment arrived)
        //        EXPECT_TRUE(totalSegmentNumber == interestNumber || 1 == interestNumber);
        // append segment for each interest in random order
        if (gotPendingInterests && frameNo < framesNumber)
        {
            LOG_TRACE("got %d interests for frame %d", interestNumber, frameNo);
            for (unsigned int i = 0; i < interestNumber; i++)
            {
                unsigned int segmentNo = remainingSegments[frameNo].front();
                unsigned int currentSegmentSize = (segmentNo == totalSegmentNumber-1) ? payloadSize - segmentSize*fullSegmentsNumber : segmentSize;
                
                remainingSegments[frameNo].erase(remainingSegments[frameNo].begin());
                
                LOG_TRACE("appending frameno %d segno %d", frameNo, segmentNo);
                // check if it's a frist segment
                if (buffer_->getState(frameNo) == FrameBuffer::Slot::StateNew)
                {
                    buffer_->markSlotAssembling(frameNo, totalSegmentNumber, segmentSize);
                }
                buffer_->appendSegment(frameNo, segmentNo, currentSegmentSize, frameSegments[segmentNo]);
                
                if (remainingSegments[frameNo].size() == 0)
                    remainingSegments.erase(remainingSegments.find(frameNo));
            }
        }
        
        stop = (remainingSegments.size() == 0);
        //        WAIT(10);
    }
    
    EXPECT_TRUE_WAIT((providerCycleCount_ == framesNumber || providerCycleCount_ == framesNumber+1), 5000);
    
    bookingThread.SetNotAlive();
    providerThread.SetNotAlive();
    
    buffer_->release();
    
    bookingThread.Stop();
    providerThread.Stop();
    
    delete buffer_;
}

TEST_F(FrameBufferTester, TestFullWithPlayoutBuffer)
{
    // there are 3 threads:
    // - booking thread (books slots in a buffer, listens to events: FirstSegment, FreeSlot, Timeout)
    // - assembling thread - appends segments (current main thread)
    // - frame playout thread (creates playout buffer based on current frame buffer and is invoked every 1/fps second)
    // buffer is smaller than the amount of frames needed to be delivered
    
    NdnFrameData frameData(*encodedFrame_);
    
    unsigned int payloadSize = frameData.getLength();
    unsigned int framesNumber = 15;
    unsigned int bufferSize = 5;
    unsigned int slotSize = 4000;
    unsigned int fullSegmentsNumber = 7;
    unsigned int segmentSize = payloadSize/fullSegmentsNumber;
    unsigned int totalSegmentNumber = (payloadSize - segmentSize*fullSegmentsNumber !=0)?fullSegmentsNumber+1:fullSegmentsNumber;
    map<unsigned int, unsigned char*> frameSegments;
    map<unsigned int, vector<unsigned int>> remainingSegments;
    
    buffer_ = new FrameBuffer();
    buffer_->init(bufferSize, slotSize);
    
    for (unsigned int i = 0; i < totalSegmentNumber; i++)
        frameSegments[i] = frameData.getData()+i*segmentSize;
    
    for (unsigned int i = 0; i < framesNumber; i++)
    {
        vector<unsigned int> currentFrameSegments;
        for (int j = 0; j < totalSegmentNumber; j++)
            currentFrameSegments.push_back(j);
        
        random_shuffle(currentFrameSegments.begin(), currentFrameSegments.end());
        remainingSegments[i] = currentFrameSegments;
    }
    
    webrtc::ThreadWrapper &bookingThread(*webrtc::ThreadWrapper::CreateThread(processBookingThread, this));
    webrtc::ThreadWrapper &playoutThread(*webrtc::ThreadWrapper::CreateThread(processPlayoutThread, this));
    //    webrtc::ThreadWrapper &providerThread(*webrtc::ThreadWrapper::CreateThread(processProviderThread, this));
    
    setupBooker();
    //    setupProvider();
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
        bookingCs_->Enter();
        // check number of sent interests per frame
        unsigned int frameNo;
        unsigned int interestNumber;
        bool gotPendingInterests = false;
        
        for (map<int, int>::iterator it = interestsPerFrame_.begin(); it != interestsPerFrame_.end(); ++it)
        {
            frameNo = it->first;
            interestNumber = it->second;
            interestsPerFrame_.erase(it);
            gotPendingInterests = true;
            
            break;
        }
        bookingCs_->Leave();
        
        // append segment for each interest in random order
        if (gotPendingInterests && frameNo < framesNumber)
        {
            LOG_TRACE("got %d interests for frame %d", interestNumber, frameNo);
            for (unsigned int i = 0; i < interestNumber; i++)
            {
                unsigned int segmentNo = remainingSegments[frameNo].front();
                unsigned int currentSegmentSize = (segmentNo == totalSegmentNumber-1) ? payloadSize - segmentSize*fullSegmentsNumber : segmentSize;
                
                remainingSegments[frameNo].erase(remainingSegments[frameNo].begin());
                
                //                TRACE("appending frameno %d segno %d", frameNo, segmentNo);
                // check if it's a frist segment
                if (buffer_->getState(frameNo) == FrameBuffer::Slot::StateNew)
                    buffer_->markSlotAssembling(frameNo,
                                                totalSegmentNumber,
                                                segmentSize);
                
                buffer_->appendSegment(frameNo, segmentNo, currentSegmentSize, frameSegments[segmentNo]);
                
                if (remainingSegments[frameNo].size() == 0)
                    remainingSegments.erase(remainingSegments.find(frameNo));
            }
        }
        
        stop = (remainingSegments.size() == 0);
    }
    
    EXPECT_TRUE_WAIT((playoutProcessed_ == framesNumber), 1000);
    
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
    
    frameBuffer->init(1, 8000);
    
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
    
    frameBuffer->init(1, 8000);
    playoutBuffer->init(frameBuffer, 30);
    
    webrtc::EncodedImage *frame = NdnRtcObjectTestHelper::loadEncodedFrame();
    ASSERT_NE((webrtc::EncodedImage*)NULL, frame);
    
    NdnFrameData frameData(*frame);
    
    FrameBuffer::Slot *slot = playoutBuffer->acquireNextSlot();
    ASSERT_EQ(nullptr, slot);
    
    // append new frame
    frameBuffer->bookSlot(frameNo);
    frameBuffer->markSlotAssembling(frameNo, 1, frameData.getLength());
    frameBuffer->appendSegment(frameNo, 0, frameData.getLength(), frameData.getData());
    
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
