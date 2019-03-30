//
// test-pipeline-control.cc
//
//  Created by Peter Gusev on 10 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <ctime>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "tests-helpers.hpp"

#include "include/name-components.hpp"
#include "src/pipeline-control.hpp"
#include "src/pipeline.hpp"
#include "src/pool.hpp"
#include "src/frame-buffer.hpp"
#include "src/interest-queue.hpp"
#include "src/proto/ndnrtc.pb.h"
#include "src/clock.hpp"

#include <ndn-cpp/name.hpp>
#include <ndn-cpp/interest.hpp>

using namespace ::testing;
using namespace std;
using namespace ndn;
using namespace ndnrtc;

//#define ENABLE_LOGGING

class InterestQueueStub : public IInterestQueue {
public:
    MOCK_METHOD5(enqueueInterest, void(const boost::shared_ptr<const ndn::Interest>& interest,
                                       boost::shared_ptr<DeadlinePriority> priority,
                                       OnData onData,
                                       OnTimeout onTimeout,
                                       OnNetworkNack onNetworkNack));
    MOCK_METHOD2(enqueueRequest, void(boost::shared_ptr<DataRequest>& request,
                                      boost::shared_ptr<DeadlinePriority> priority));
    MOCK_METHOD2(enqueueRequests, void(std::vector<boost::shared_ptr<DataRequest>>& requests,
                                       boost::shared_ptr<DeadlinePriority> priority));
    MOCK_METHOD0(reset, void());
};

TEST(TestPipeline, TestDefault)
{
    // create frame pool
    int dispatchedSlotsNum = 0;
    Pool<BufferSlot> slotPool(500);
    auto dispatchSlotFun = [&]() -> boost::shared_ptr<IPipelineSlot>{
        dispatchedSlotsNum++;
        return slotPool.pop();
    };
    
    Name prefix = NameComponents::streamPrefix("/my/stream/prefix", "mystream");
    boost::shared_ptr<InterestQueueStub> iqStub = boost::make_shared<InterestQueueStub>();
    Pipeline pp(iqStub, dispatchSlotFun, prefix, 0);

    int nNewSlots = 0;
    uint64_t lastSeqNo = 0;
    pp.onNewSlot.connect([&](boost::shared_ptr<IPipelineSlot> slot){
        nNewSlots++;
        lastSeqNo = slot->getPrefix()[-1].toSequenceNumber();
        EXPECT_EQ(slot->getState(), PipelineSlotState::New);
    });

    int nPulses = 10;
    EXPECT_CALL(*iqStub, enqueueRequests(_,_))
        .Times(AtLeast(nPulses));
    
    for (int i = 0; i < nPulses; ++i)
        pp.pulse();
    EXPECT_EQ(nNewSlots, nPulses);
    EXPECT_EQ(lastSeqNo, nPulses-1);
}

TEST(TestPipeline, TestSeed)
{
    // create frame pool
    int dispatchedSlotsNum = 0;
    Pool<BufferSlot> slotPool(500);
    auto dispatchSlotFun = [&]() -> boost::shared_ptr<IPipelineSlot>{
        dispatchedSlotsNum++;
        return slotPool.pop();
    };
    
    Name prefix = NameComponents::streamPrefix("/my/stream/prefix", "mystream");
    boost::shared_ptr<InterestQueueStub> iqStub = boost::make_shared<InterestQueueStub>();
    int seed = 100;
    Pipeline pp(iqStub, dispatchSlotFun, prefix, seed);
    
    int nNewSlots = 0;
    uint64_t lastSeqNo = 0;
    pp.onNewSlot.connect([&](boost::shared_ptr<IPipelineSlot> slot){
        nNewSlots++;
        lastSeqNo = slot->getPrefix()[-1].toSequenceNumber();
        EXPECT_EQ(slot->getState(), PipelineSlotState::New);
    });
    
    int nPulses = 10;
    EXPECT_CALL(*iqStub, enqueueRequests(_,_))
    .Times(nPulses);
    
    for (int i = 0; i < nPulses; ++i)
        pp.pulse();
    EXPECT_EQ(nNewSlots, nPulses);
    EXPECT_EQ(lastSeqNo, seed+nPulses-1);
}

TEST(TestPipeline, TestStep)
{
    // create frame pool
    int dispatchedSlotsNum = 0;
    Pool<BufferSlot> slotPool(500);
    auto dispatchSlotFun = [&]() -> boost::shared_ptr<IPipelineSlot>{
        dispatchedSlotsNum++;
        return slotPool.pop();
    };
    
    Name prefix = NameComponents::streamPrefix("/my/stream/prefix", "mystream");
    boost::shared_ptr<InterestQueueStub> iqStub = boost::make_shared<InterestQueueStub>();
    int step = 30;
    Pipeline pp(iqStub, dispatchSlotFun, prefix, 0, 30);
    
    int nNewSlots = 0;
    uint64_t lastSeqNo = 0;
    pp.onNewSlot.connect([&](boost::shared_ptr<IPipelineSlot> slot){
        nNewSlots++;
        lastSeqNo = slot->getPrefix()[-1].toSequenceNumber();
        EXPECT_EQ(slot->getState(), PipelineSlotState::New);
    });
    
    int nPulses = 10;
    EXPECT_CALL(*iqStub, enqueueRequests(_,_))
        .Times(nPulses);
    
    for (int i = 0; i < nPulses; ++i)
        pp.pulse();
    EXPECT_EQ(nNewSlots, nPulses);
    EXPECT_EQ(lastSeqNo, step*(nPulses-1));
}

TEST(TestPipeline, TestStepBckwd)
{
    // create frame pool
    int dispatchedSlotsNum = 0;
    Pool<BufferSlot> slotPool(500);
    auto dispatchSlotFun = [&]() -> boost::shared_ptr<IPipelineSlot>{
        dispatchedSlotsNum++;
        return slotPool.pop();
    };
    
    Name prefix = NameComponents::streamPrefix("/my/stream/prefix", "mystream");
    boost::shared_ptr<InterestQueueStub> iqStub = boost::make_shared<InterestQueueStub>();
    int step = -1;
    int seed = 100;
    Pipeline pp(iqStub, dispatchSlotFun, prefix, seed, step);
    
    int nNewSlots = 0;
    uint64_t lastSeqNo = 0;
    pp.onNewSlot.connect([&](boost::shared_ptr<IPipelineSlot> slot){
        nNewSlots++;
        lastSeqNo = slot->getPrefix()[-1].toSequenceNumber();
        EXPECT_EQ(slot->getState(), PipelineSlotState::New);
    });
    
    int nPulses = 10;
    EXPECT_CALL(*iqStub, enqueueRequests(_,_))
        .Times(AtLeast(nPulses));
    
    for (int i = 0; i < nPulses; ++i)
        pp.pulse();
    EXPECT_EQ(nNewSlots, nPulses);
    EXPECT_EQ(lastSeqNo, seed - (nPulses-1));
}

// ------------------------------------------------------------------------------------------------
TEST(TestPipeline, TestIntegrationFullCycle)
{
    if (!checkNfd()) return;
    
    string basePrefix("/my/base/prefix");
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    KeyChain kc;
    Face pFace;
    Face cFace;
    FaceStub faceStub;
    
    map<Name, boost::shared_ptr<Data>> framePackets;
    uint64_t startSeqNo = 0, nFrames = 1;
    for (uint64_t f = startSeqNo; f < nFrames; ++f)
    {
        Name frameName = Name(stream).appendSequenceNumber(f);
        vector<boost::shared_ptr<Data>> packets = getSampleFrameData(frameName);
        
        for (auto &d:packets)
            framePackets[d->getName()] = d;
    }
    
    // --------------------------------------------------------------------------------------------
    // MOCK PRODUCER SIDE
    pFace.setCommandSigningInfo(kc, kc.getDefaultCertificateName());
    pFace.registerPrefix(basePrefix,
                         bind(&FaceStub::onInterest, &faceStub, _1, _2, _3, _4, _5),
                         bind(&FaceStub::onRegisterFailure, &faceStub, _1),
                         bind(&FaceStub::onRegisterSuccess, &faceStub, _1, _2));
    
    bool prefixRegistered = false;
    EXPECT_CALL(faceStub, onRegisterSuccess(_,_))
    .Times(1)
    .WillOnce(Invoke([&](const boost::shared_ptr<const Name>&,
                         uint64_t){
        prefixRegistered = true;
    }));
    EXPECT_CALL(faceStub, onRegisterFailure(_))
    .Times(0);
    EXPECT_CALL(faceStub, onInterest(_,_,_,_,_))
    .WillRepeatedly(Invoke([&](const boost::shared_ptr<const Name>& prefix,
                               const boost::shared_ptr<const Interest>& interest, Face& face,
                               uint64_t interestFilterId,
                               const boost::shared_ptr<const InterestFilter>& filter)
                           {
                               if (framePackets.find(interest->getName()) == framePackets.end())
                                   FAIL() << "can't request frame " << prefix->toUri();
                               else
                                   face.putData(*framePackets[interest->getName()]);
                           }));
    
    int delay = 100000;
    int64_t start = clock::millisecondTimestamp();
    while (!prefixRegistered && clock::millisecondTimestamp() - start < delay)
    {
        pFace.processEvents();
        usleep(10);
    }
    
    // --------------------------------------------------------------------------------------------
    // MOCK CONSUMER SIDE
    bool useParity = true;
    boost::asio::io_service io;
    boost::shared_ptr<InterestQueue> interestQ = boost::make_shared<InterestQueue>(io, &cFace);
    Pool<BufferSlot> slotPool(500);
    int nPending = 0;
    auto dispatchSlotFun = [&]()->boost::shared_ptr<IPipelineSlot>{
        nPending ++ ;
        boost::shared_ptr<IPipelineSlot> slot = slotPool.pop();
        slot->subscribe(PipelineSlotState::Pending, [](const IPipelineSlot* slot, const DataRequest &dr){
#ifdef ENABLE_LOGGING
            LogTrace("") << "slot pending " << slot->getPrefix() << endl;
#endif
        });
        return slot;
    };
    

    Pipeline pp(interestQ, dispatchSlotFun, stream, startSeqNo);
#ifdef ENABLE_LOGGING
    ndnlog::new_api::Logger::initAsyncLogging();
    ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
    
    interestQ->setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
    pp.setLogger(ndnlog::new_api::Logger::getLoggerPtr(""));
#endif

    int nNewSlots = 0, nReadySlots = 0, nSlotsWithMissing = 0;
    vector<boost::shared_ptr<IPipelineSlot>> slotBuffer;
    
    auto onSlotReady = [&](const IPipelineSlot* slot, const DataRequest& dr){
        nReadySlots++;
#ifdef ENABLE_LOGGING
        LogTrace("") << "slot " << slot->getPrefix() << " is ready" << endl;
#endif
        // pulse pipeline whenever slot is ready
        pp.pulse();
        vector<boost::shared_ptr<IPipelineSlot>>::iterator it = slotBuffer.begin();
        while (it != slotBuffer.end())
        {
            if ((*it)->getPrefix() == slot->getPrefix())
            {
                boost::shared_ptr<BufferSlot> s = boost::dynamic_pointer_cast<BufferSlot>(*it);
                
                slotPool.push(s);
                slotBuffer.erase(it);
                
                break;
            }
            ++it;
        }
    };
    auto onSlotUnfetchable = [&](const IPipelineSlot* slot, const DataRequest& dr){
        FAIL() << "Slot " << slot->getPrefix() << " is unfetchable";
    };
    auto onMissingSlotRequests = [&](IPipelineSlot *slot,
                                     std::vector<boost::shared_ptr<DataRequest>> requests)
    {
#ifdef ENABLE_LOGGING
        LogTrace("") << "slot " << slot->getPrefix() << " needs " << requests.size() << " more segment(s)" << endl;
#endif
        vector<boost::shared_ptr<DataRequest>> expressedRequests;
        
        nSlotsWithMissing++;
        for (auto &dr:requests)
        {
            if (dr->getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
            {
                // model optional parity data fetching
                if (useParity)
                {
                    expressedRequests.push_back(dr);
                    interestQ->enqueueRequest(dr, boost::make_shared<DeadlinePriority>(0));
                }
            }
            else
            {
                expressedRequests.push_back(dr);
                interestQ->enqueueRequest(dr, boost::make_shared<DeadlinePriority>(0));
            }
        }
        
        slot->setRequests(expressedRequests);
    };
    
    pp.onNewSlot.connect([&](boost::shared_ptr<IPipelineSlot> slot){
        nNewSlots++;
        slotBuffer.push_back(slot);
        slot->subscribe(PipelineSlotState::Ready, onSlotReady);
        slot->subscribe(PipelineSlotState::Unfetchable, onSlotUnfetchable);
        
        boost::shared_ptr<BufferSlot> s = boost::dynamic_pointer_cast<BufferSlot>(slot);
        slot->addOnNeedData(onMissingSlotRequests);
    });
    
    start = clock::millisecondTimestamp();
    pp.pulse();
    do
    {
        pFace.processEvents();
        cFace.processEvents();
        io.run();
        if (io.stopped())
            io.restart();
        usleep(1);
    } while (clock::millisecondTimestamp() - start < delay && nReadySlots < nFrames);
    
    
    EXPECT_EQ(nPending, nFrames+1);
    EXPECT_EQ(nNewSlots, nFrames+1);
    EXPECT_EQ(nReadySlots, nFrames);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
