//
//  video-receiver-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#define DEBUG

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_TRACE

#include "test-common.h"
#include "video-receiver.h"

using namespace ndnrtc;

//********************************************************************************
// Slot tests
class FrameSlotTester : public NdnRtcObjectTestHelper
{
    public :
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
        
        encodedFrame_ = NdnRtcObjectTestHelper::loadEncodedFrame();
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        delete encodedFrame_->_buffer;
        delete encodedFrame_;
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
            
            FrameBuffer::Slot::State state = slot->appendSegment(actualSegmentsNum-i, size, segments[i]);
            
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
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
        
        delete encodedFrame_->_buffer;
        delete encodedFrame_;
    }
    
protected:    
    bool finishedWaiting_, fullyBooked_;
    webrtc::EncodedImage *encodedFrame_;
    
    // test thread functions
    unsigned int bookingNo_;
    int expectedBufferEvents_;
    FrameBuffer::Event receivedEvent_;
    FrameBuffer *buffer_;
    
    static bool waitBufferEvent(void *obj) { return ((FrameBufferTester*)obj)->waitBuffer(); }
    bool waitBuffer()
    {
//        finishedWaiting_ = false;
        receivedEvent_ = buffer_->waitForEvents(expectedBufferEvents_);
        finishedWaiting_ = true;
        
        return true;
    }
    
    static bool waitBufferEventAndBookSlot(void *obj) { return ((FrameBufferTester*)obj)->waitBufferAndBook(); }
    bool waitBufferAndBook()
    {
//        finishedWaiting_ = false;
        receivedEvent_ = buffer_->waitForEvents(expectedBufferEvents_);
        finishedWaiting_ = true;
        
        {
            EXPECT_EQ(FrameBuffer::CallResultNew, buffer_->bookSlot(bookingNo_++));
        }

        if (bookingNo_ == buffer_->getBufferSize())
        {
            EXPECT_EQ(FrameBuffer::CallResultFull, buffer_->bookSlot(bookingNo_));
            fullyBooked_ = true;
        }
        
        return true; //!fullyBooked_;
    }
    
    
    // booker thread mimics ndn-rtc real-app thread which pipelines interests for the segments and frames
    vector<int> bookedFrames_;
    map<int, int> interestsPerFrame_;
    unsigned int lastBooked_, bookingLimit_, bookerCycleCount_, bookerWaitingEvents_;
    bool bookerGotEvent_, bookerFilledBuffer_;
    vector<FrameBuffer::Event> bookerEvents_;
    
    static bool processBookingThread(void *obj) { return ((FrameBufferTester*)obj)->processBooking(); }
    bool processBooking()
    {
        bookerCycleCount_++;
        FrameBuffer::Event ev = buffer_->waitForEvents(bookerWaitingEvents_);
        bookerGotEvent_ = true;
        
        bookerEvents_.push_back(ev);
        
        FrameBuffer::CallResultNew res;
        
        switch (ev.type_) {
            case FrameBuffer::Event::EventTypeFreeSlot:
                bookerFilledBuffer_ = false;
                res = buffer_->bookSlot(lastBooked_++);
                break;
            case FrameBuffer::Event::EventTypeTimeout:
                // in real app, re-issue interest here (if needed): "new interest(ev.frameNo_, ev.segmentNo_)"
                break;
            case FrameBuffer::Event::EventTypeFirstSegment:
                // in real app, interest pipelining should be established here
                // should check ev.slot_->totalSegmentsNumber() and ev.segmentNo_
                interestsPerFrame_[ev.frameNo_] = ev.slot_->totalSegmentsNumber() - 1;
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
    
    
    
    static bool processAssemblingThread(void *obj) { return ((FrameBufferTester*)obj)->processAssembling(); }
    bool processAssembling()
    {
        
    }
    
    // in real app, frame provider thread (or playout thread) is not waiting on buffer events,
    // instead - it asks buffer for specific frames with constant frequency
    static bool processProviderThread(void *obj) { return ((FrameBufferTester*)obj)->processProvider(); }
    bool processProvider()
    {
        
    }
    
    virtual void flushFlags()
    {
        NdnRtcObjectTestHelper::flushFlags();
        finishedWaiting_ = false;
        fullyBooked_ = false;
        bookingNo_ = 0;
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
        
        delete buffer;
    }
    
    { // test book twice
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;

        buffer->init(buffSize, slotSize);
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(32));
        
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
        buffer->markSlotFree(32);
        EXPECT_EQ(FrameBuffer::CallResultFull, buffer->bookSlot(11));
        
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
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(32));
        
        buffer->markSlotFree(32);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        
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

        buffer->lockSlot(32);
        // shouldn't be freed
        buffer->markSlotFree(32);
        
        EXPECT_EQ(FrameBuffer::CallResultBooked, buffer->bookSlot(32));
        
        delete buffer;
    }
    
    { // standard sequence
        flushFlags();
        
        FrameBuffer *buffer = new FrameBuffer();
        
        unsigned int buffSize = 2;
        unsigned int slotSize = 8000;
        
        buffer->init(buffSize, slotSize);
        
        EXPECT_EQ(FrameBuffer::CallResultNew, buffer->bookSlot(32));
        
        buffer->lockSlot(32);
        buffer->unlockSlot(32);

        buffer->markSlotFree(32);
        
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
        
        unsigned int buffSize = 60;
        unsigned int slotSize = 8000;
        
        buffer_->init(buffSize, slotSize);
        
        webrtc::ThreadWrapper &waitingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
        
        expectedBufferEvents_ = FrameBuffer::Event::AllEventsMask;
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 100);
        
        waitingThread.SetNotAlive();
        waitingThread.Stop();
        
        EXPECT_EQ(FrameBuffer::Event::EventTypeFreeSlot,receivedEvent_.type_);
        
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
        waitingThread.SetNotAlive();
//        waitingThread.Stop();

        EXPECT_EQ(FrameBuffer::Event::EventTypeFreeSlot,receivedEvent_.type_);
        EXPECT_EQ(7, receivedEvent_.frameNo_);
        
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
        //        waitingThread.Stop();
        
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
        //        waitingThread.Stop();
        
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
        
        expectedBufferEvents_ = FrameBuffer::Event::EventTypeReady;
        buffer_->bookSlot(frameNo);
        
        unsigned int tid = 1;
        waitingThread.Start(tid);
        
        unsigned char segment[100];
        
        buffer_->markSlotAssembling(frameNo, segmentsNum, 100);
        EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(frameNo, segmentNo, 100, segment));
        
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::EventTypeReady, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
        EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
        
        waitingThread.SetNotAlive();
        //        waitingThread.Stop();
        
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
        
        unsigned char segment[100];
        
        buffer_->markSlotAssembling(frameNo, segmentsNum, 100);
        
        for (int i = 0; i < segmentsNum; i++)
        {
            EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(frameNo, i, 100, segment));
            EXPECT_FALSE(finishedWaiting_);
        }
        
        EXPECT_FALSE(finishedWaiting_);
        EXPECT_TRUE_WAIT(finishedWaiting_, 500);
        EXPECT_EQ(FrameBuffer::Event::EventTypeReady, receivedEvent_.type_);
        EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
        EXPECT_EQ(lastSegmentNo, receivedEvent_.segmentNo_);
        
        waitingThread.SetNotAlive();
        //        waitingThread.Stop();
        
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
        
        waitingThread.SetNotAlive();
        //        waitingThread.Stop();
        
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
        
        { // timeout
            flushFlags();
            buffer_->notifySegmentTimeout(frameNo, segmentNo);
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::EventTypeTimeout, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
            EXPECT_EQ(segmentNo, receivedEvent_.segmentNo_);
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
        }
        
        { // ready
            flushFlags();
            unsigned char segment[100];
            
            for (int i = 1; i < segmentsNum; i++)
            {
                EXPECT_EQ(FrameBuffer::CallResultAssembling, buffer_->appendSegment(frameNo, i, 100, segment));
                EXPECT_FALSE(finishedWaiting_);
            }
            
            EXPECT_TRUE_WAIT(finishedWaiting_, 500);
            EXPECT_EQ(FrameBuffer::Event::EventTypeReady, receivedEvent_.type_);
            EXPECT_EQ(frameNo, receivedEvent_.frameNo_);
            EXPECT_EQ(segmentsNum-1, receivedEvent_.segmentNo_);
        }
        
        waitingThread.SetNotAlive();
        //        waitingThread.Stop();
        
        delete buffer_;
    }
}
TEST_F(FrameBufferTester, TestFull)
{
    // there is going to be 3 threads:
    // - booking thread (books slots in a buffer, listens to events: FirstSegment, FreeSlot, Timeout)
    // - assembling thread (appends segments)
    // - frame provider thread (supplies full assembled frames by locking them for random small
    //      amount of time, listens to events: Ready)
    // buffer will be smaller than the ammount of frames needed to be delivered
    
    unsigned int framesNumber = 15;
    unsigned int bufferSize = 5;
    
    webrtc::ThreadWrapper &bookingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
    webrtc::ThreadWrapper &assemblingThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
    webrtc::ThreadWrapper &providerThread(*webrtc::ThreadWrapper::CreateThread(waitBufferEvent, this));
}
//********************************************************************************
// VideoReceiver tests
class NdnReceiverTester : public NdnRtcObjectTestHelper
{
public :
    void SetUp()
    {
        NdnRtcObjectTestHelper::SetUp();
    }
    void TearDown()
    {
        NdnRtcObjectTestHelper::TearDown();
    }
    
protected:
    
};

TEST_F(NdnReceiverTester, CreateDelete)
{
    NdnVideoReceiver *receiver = new NdnVideoReceiver(VideoSenderParams::defaultParams());
    delete receiver;
}

