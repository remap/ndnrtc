//
// test-frame-buffer.cc
//
//  Created by Peter Gusev on 28 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <algorithm>
#include <ctime>

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/name.hpp>

#include "src/frame-buffer.hpp"
#include "tests-helpers.hpp"

using namespace std;
using namespace std::placeholders;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;
using namespace testing;

class SlotCallbacksStub {
public:
    MOCK_METHOD2(onPending, void(const IPipelineSlot*, const DataRequest&));
    MOCK_METHOD2(onReady, void(const IPipelineSlot*, const DataRequest&));
    MOCK_METHOD2(onUnfetchable, void(const IPipelineSlot*, const DataRequest&));
    MOCK_METHOD2(onMissing, void(IPipelineSlot*, vector<shared_ptr<DataRequest>>));
};

TEST(TestBufferSlot, TestCreate)
{
    string basePrefix = "/my/base/prefix";
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    Name frame = Name(stream).appendSequenceNumber(0);
    vector<shared_ptr<Data>> packets = getSampleFrameData(frame);
    vector<shared_ptr<DataRequest>> requests;
    int lifetime = 30;

    { // meta
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Meta), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // manifest
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Manifest), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // data
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // parity
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Parity).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

    EXPECT_EQ(slot->getState(), PipelineSlotState::Free);
    EXPECT_EQ(slot->getPrefix().toUri(), "/");

    slot->setRequests(requests);
    EXPECT_EQ(slot->getPrefix().toUri(), frame.toUri());
    EXPECT_EQ(slot->getState(), PipelineSlotState::New);

    EXPECT_EQ(slot->getShortestDrd(), 0);
    EXPECT_EQ(slot->getLongestDrd(), 0);
    EXPECT_EQ(slot->getFetchProgress(), 0);
}

TEST(TestBufferSlot, TestIsDecodable)
{
    string basePrefix = "/my/base/prefix";
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    Name frame = Name(stream).appendSequenceNumber(0);
    vector<shared_ptr<Data>> packets = getSampleFrameData(frame);
    vector<shared_ptr<DataRequest>> requests;
    int lifetime = 30;
    auto expressRequests = [](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &r:requests)
        {
            p.timestampRequest(*r);
            p.setStatus(DataRequest::Status::Expressed, *r);
            p.triggerEvent(DataRequest::Status::Expressed, *r);
        }
    };
    auto replyData = [packets](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &d:packets)
        {
            for (auto &r:requests)
            {
                if (d->getName().match(r->getInterest()->getName()))
                {
                    p.timestampReply(*r);
                    p.setData(d, *r);
                    p.setStatus(DataRequest::Status::Data, *r);
                    p.triggerEvent(DataRequest::Status::Data, *r);
                }
            }
        }
    };
    vector<shared_ptr<DataRequest>> missing;
    auto onMissing = [&](IPipelineSlot* s, vector<shared_ptr<DataRequest>> requests){
        EXPECT_GT(requests.size(), 0);
        missing.insert(missing.end(), requests.begin(), requests.end());
        s->setRequests(requests);
        expressRequests(requests);
    };

    { // meta
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Meta), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // manifest
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Manifest), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // data
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // parity
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Parity).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    { // reply all data
        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
        slot->setRequests(requests);
        slot->addOnNeedData(onMissing);
        expressRequests(requests);
        replyData(requests);
        replyData(missing);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
        EXPECT_TRUE(slot->isReadyForDecoder());
    }
    {// reply only payload data
        missing.clear();
        auto replyNoParity = [&](vector<shared_ptr<DataRequest>> requests){
            UnitTestDataRequestProxy p;
            for (auto &d:packets)
                for (auto &r:requests)
                    if (r->getNamespaceInfo().segmentClass_ != SegmentClass::Parity)
                    {
                        if (d->getName().match(r->getInterest()->getName()))
                        {
                            p.timestampReply(*r);
                            p.setData(d, *r);
                            p.setStatus(DataRequest::Status::Data, *r);
                            p.triggerEvent(DataRequest::Status::Data, *r);
                        }
                    }
        };

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
        slot->setRequests(requests);
        slot->addOnNeedData(onMissing);
        expressRequests(requests);
        replyNoParity(requests);
        replyNoParity(missing);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
        EXPECT_TRUE(slot->isReadyForDecoder());
    }
    { // reply all parity and some payload
        missing.clear();
        int nParity = 0, nData = 0;
        for_each(packets.begin(), packets.end(), [&](shared_ptr<Data> d){
            if (d->getName()[-2] == NameComponents::Parity) nParity++;
            if (d->getName()[-1].isSegment() && d->getName()[-2].isSequenceNumber()) nData++;
        });

        assert(nParity >= 2);
        if (nParity >= 2) // skip test if parity is less than 2
        {
            int nDataToSkip = nParity/2;
            assert(nDataToSkip >= 1);
            auto replyAllParity = [&nDataToSkip, &packets](vector<shared_ptr<DataRequest>> requests){
                UnitTestDataRequestProxy p;
                for (auto &r:requests)
                {
                    if (nDataToSkip <= 0 ||
                        r->getNamespaceInfo().segmentClass_ != SegmentClass::Data)
                    {
                        for (auto &d:packets)
                        {
                                if (d->getName().match(r->getInterest()->getName()))
                                {
                                    p.timestampReply(*r);
                                    p.setData(d, *r);
                                    p.setStatus(DataRequest::Status::Data, *r);
                                    p.triggerEvent(DataRequest::Status::Data, *r);
                                }
                        }
                    }
                    else
                        nDataToSkip--;
                }
            };

            shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
            slot->setRequests(requests);
            slot->addOnNeedData(onMissing);
            expressRequests(requests);
            replyData(requests);
            replyAllParity(missing);

            EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
            EXPECT_TRUE(slot->isReadyForDecoder());
            EXPECT_EQ(slot->getDataSegmentsNum(), nData - nDataToSkip);
            EXPECT_EQ(slot->getParitySegmentsNum(), nParity);
        }
    }
    { // first reply - manifest
        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
        slot->setRequests(requests);
        slot->addOnNeedData(onMissing);
        expressRequests(requests);

        vector<shared_ptr<DataRequest>> manifest;
        vector<shared_ptr<DataRequest>> other;

        for_each(requests.begin(), requests.end(), [&manifest, &other](shared_ptr<DataRequest> r){
            if (r->getNamespaceInfo().segmentClass_ == SegmentClass::Manifest)
                manifest.push_back(r);
            else
                other.push_back(r);
        });

        replyData(manifest);
        EXPECT_FALSE(slot->isReadyForDecoder());
        replyData(other);
        replyData(missing);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
        EXPECT_TRUE(slot->isReadyForDecoder());
    }
    { // first reply - parity
        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
        slot->setRequests(requests);
        slot->addOnNeedData(onMissing);
        expressRequests(requests);

        vector<shared_ptr<DataRequest>> parity;
        vector<shared_ptr<DataRequest>> other;

        for_each(requests.begin(), requests.end(), [&parity, &other](shared_ptr<DataRequest> r){
            if (r->getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
                parity.push_back(r);
            else
                other.push_back(r);
        });

        replyData(parity);
        EXPECT_FALSE(slot->isReadyForDecoder());
        replyData(other);
        replyData(missing);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
        EXPECT_TRUE(slot->isReadyForDecoder());
    }
}

TEST(TestBufferSlot, TestFrameAssembling)
{
    string basePrefix = "/my/base/prefix";
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    Name frame = Name(stream).appendSequenceNumber(0);
    int lifetime = 30;
    vector<shared_ptr<Data>> packets = getSampleFrameData(frame);
    auto expressRequests = [](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &r:requests)
        {
            p.timestampRequest(*r);
            p.setStatus(DataRequest::Status::Expressed, *r);
            p.triggerEvent(DataRequest::Status::Expressed, *r);
        }
    };
    auto replyData = [packets](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &d:packets)
        {
            for (auto &r:requests)
            {
                if (d->getName().match(r->getInterest()->getName()))
                {
                    p.timestampReply(*r);
                    p.setData(d, *r);
                    p.setStatus(DataRequest::Status::Data, *r);
                    p.triggerEvent(DataRequest::Status::Data, *r);
                }
            }
        }
    };
    vector<shared_ptr<DataRequest>> missing;
    auto onMissing = [&](IPipelineSlot* s, vector<shared_ptr<DataRequest>> requests){
        EXPECT_GT(requests.size(), 0);
        missing.insert(missing.end(), requests.begin(), requests.end());
        s->setRequests(requests);
        expressRequests(requests);
    };

    vector<shared_ptr<DataRequest>> requests;
    { // meta
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Meta), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // manifest
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Manifest), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // data
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // parity
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Parity).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    {   // TEST SLOT READY
        SlotCallbacksStub callbackStub;
        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onMissing));

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));
        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(0);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply initial data
        replyData(requests);
        EXPECT_GT(missing.size(), 0);

        // -- reply missing data
        replyData(missing);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
    }

    { // TEST SLOT READY WHEN EXPRESS MORE THAN NEEDED REQUESTS
        SlotCallbacksStub callbackStub;

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));

        // set requests
        int nParity = 0, nData = 0;
        for_each(packets.begin(), packets.end(), [&](shared_ptr<Data> d){
            if (d->getName()[-2] == NameComponents::Parity) nParity++;
            if (d->getName()[-1].isSegment() && d->getName()[-2].isSequenceNumber()) nData++;
        });
        vector<shared_ptr<DataRequest>> moreRequests(requests.begin(), requests.end());
        for (int i = 0; i < requests.size(); ++i)
        {
            if (requests[i]->getNamespaceInfo().segmentClass_ == SegmentClass::Data ||
                requests[i]->getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
            {
                int startSeg = (requests[i]->getNamespaceInfo().segmentClass_ == SegmentClass::Data ? nData : nParity);
                Name segPrefix = Name(frame);
                if (requests[i]->getNamespaceInfo().segmentClass_ == SegmentClass::Parity)
                    segPrefix.append(NameComponents::Parity);

                // add two more requests
                for (int seg = 1; seg < startSeg + 2; ++seg)
                {
                    auto itrst = make_shared<Interest>(Name(segPrefix).appendSegment(seg), lifetime);
                    itrst->setMustBeFresh(false);
                    moreRequests.push_back(make_shared<DataRequest>(itrst));
                }
            }
        }

        slot->setRequests(moreRequests);

        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(0);
        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(0);

        // express initial requests
        expressRequests(moreRequests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply data
        replyData(moreRequests);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
    }

    {   // TEST SLOT UNFETCHABLE - TIMEOUT
        missing.clear();
        SlotCallbacksStub callbackStub;
        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onMissing));

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));
        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(0);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(1);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply initial data
        replyData(requests);
        EXPECT_GT(missing.size(), 0);

        // -- reply timeouts
        UnitTestDataRequestProxy p;
        for (auto &r:missing)
        {
            p.timestampReply(*r);
            p.setTimeout(*r);
            p.setStatus(DataRequest::Status::Timeout, *r);
            p.triggerEvent(DataRequest::Status::Timeout, *r);
        }

        EXPECT_EQ(slot->getState(), PipelineSlotState::Unfetchable);
    }
    {   // TEST SLOT UNFETCHABLE - NACK
        missing.clear();
        SlotCallbacksStub callbackStub;
        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onMissing));

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));
        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(0);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(1);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply initial data
        replyData(requests);
        EXPECT_GT(missing.size(), 0);

        // -- reply network nacks
        UnitTestDataRequestProxy p;
        for (auto &r:missing)
        {
            p.timestampReply(*r);
            p.setNack(make_shared<NetworkNack>(), *r);
            p.setStatus(DataRequest::Status::NetworkNack, *r);
            p.triggerEvent(DataRequest::Status::NetworkNack, *r);
        }

        EXPECT_EQ(slot->getState(), PipelineSlotState::Unfetchable);
    }
    {   // TEST SLOT UNFETCHABLE - APP NACK
        missing.clear();
        SlotCallbacksStub callbackStub;
        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onMissing));

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));
        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(0);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(1);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply initial data
        replyData(requests);
        EXPECT_GT(missing.size(), 0);

        // -- reply app nacks
        UnitTestDataRequestProxy p;
        for (auto &r:missing)
        {
            shared_ptr<Data> appNack = make_shared<Data>(r->getInterest()->getName());
            appNack->getMetaInfo().setType(ndn_ContentType_NACK);
            p.timestampReply(*r);
            p.setData(appNack, *r);
            p.setStatus(DataRequest::Status::AppNack, *r);
            p.triggerEvent(DataRequest::Status::AppNack, *r);
        }

        EXPECT_EQ(slot->getState(), PipelineSlotState::Unfetchable);
    }
}

TEST(TestBufferSlot, TestFrameAssemblingEdgeCase1)
{
    string basePrefix = "/my/base/prefix";
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    Name frame = Name(stream).appendSequenceNumber(0);
    int lifetime = 30;
    vector<shared_ptr<Data>> packets = getSampleFrameData(frame, 1, 1);
    auto expressRequests = [](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &r:requests)
        {
            p.timestampRequest(*r);
            p.setStatus(DataRequest::Status::Expressed, *r);
            p.triggerEvent(DataRequest::Status::Expressed, *r);
        }
    };
    auto replyData = [packets](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &d:packets)
        {
            for (auto &r:requests)
            {
                if (d->getName().match(r->getInterest()->getName()))
                {
                    p.timestampReply(*r);
                    p.setData(d, *r);
                    p.setStatus(DataRequest::Status::Data, *r);
                    p.triggerEvent(DataRequest::Status::Data, *r);
                }
            }
        }
    };

    vector<shared_ptr<DataRequest>> requests;
    { // data
        // request more than 1 segment
        for (int seg = 0; seg < 3; ++seg)
        {
            auto i =
                make_shared<Interest>(Name(frame).appendSegment(seg), lifetime);
            i->setMustBeFresh(false);
            requests.push_back(make_shared<DataRequest>(i));
        }
    }
    { // parity
        for (int seg = 0; seg < 2; ++seg)
        {
            auto i = make_shared<Interest>(Name(frame).append(NameComponents::Parity).appendSegment(seg), lifetime);            i->setMustBeFresh(false);
            requests.push_back(make_shared<DataRequest>(i));
        }
    }
    { // meta
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Meta), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // manifest
        shared_ptr<Interest> i =
        make_shared<Interest>(Name(frame).append(NameComponents::Manifest), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    { // TEST SLOT READY WHEN EXPRESS MORE THAN NEEDED REQUESTS
        SlotCallbacksStub callbackStub;
        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();
        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));

        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(0);
        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(0);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply data
        replyData(requests);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
    }
}

class DecodeQueueTesting : public DecodeQueue {
public:
    class BitstreamDataTesting : public DecodeQueue::BitstreamData {};
};

TEST(TestBitstreamData, TestLoadFromSlot)
{
    string basePrefix = "/my/base/prefix";
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    Name frame = Name(stream).appendSequenceNumber(0);
    int lifetime = 30;
    vector<shared_ptr<Data>> packets = getSampleFrameData(frame);
    auto expressRequests = [](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &r:requests)
        {
            p.timestampRequest(*r);
            p.setStatus(DataRequest::Status::Expressed, *r);
            p.triggerEvent(DataRequest::Status::Expressed, *r);
        }
    };
    auto replyData = [packets](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &d:packets)
        {
            for (auto &r:requests)
            {
                if (d->getName().match(r->getInterest()->getName()))
                {
                    p.timestampReply(*r);
                    p.setData(d, *r);
                    p.setStatus(DataRequest::Status::Data, *r);
                    p.triggerEvent(DataRequest::Status::Data, *r);
                }
            }
        }
    };
    vector<shared_ptr<DataRequest>> missing;
    auto onMissing = [&](IPipelineSlot* s, vector<shared_ptr<DataRequest>> requests){
        EXPECT_GT(requests.size(), 0);
        missing.insert(missing.end(), requests.begin(), requests.end());
        s->setRequests(requests);
        expressRequests(requests);
    };

    vector<shared_ptr<DataRequest>> requests;
    { // meta
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Meta), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // manifest
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Manifest), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // data
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // parity
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Parity).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    int payloadSize = 0, paritySize = 0;
    int nData = 0, nParity = 0;
    int idx = 0;
    for (auto& d : packets)
    {
        if (d->getName()[-2].toEscapedString() != NameComponents::Parity)
        {
            if (d->getName()[-1].isSegment())
            {
                payloadSize += d->getContent().size();
                nData ++;
            }
        }
        else
        {
            paritySize += d->getContent().size();
            nParity ++;
        }
    }

    {   // TEST SLOT READY
        SlotCallbacksStub callbackStub;
        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onMissing));

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));
        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(0);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply initial data
        replyData(requests);
        EXPECT_GT(missing.size(), 0);

        // -- reply missing data
        replyData(missing);

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);
        EXPECT_EQ(slot->getDataSegmentsNum(), nData);
        EXPECT_EQ(slot->getFetchedDataSegmentsNum(), nData);
        EXPECT_EQ(slot->getParitySegmentsNum(), nParity);
        EXPECT_EQ(slot->getFetchedParitySegmentsNum(), nParity);

        // load data from slot
        DecodeQueueTesting::BitstreamDataTesting bsData_;

        bool recovered = false;
        shared_ptr<const BufferSlot> cSlot(slot);
        EncodedFrame ef = bsData_.loadFrom(cSlot, recovered);

        EXPECT_FALSE(recovered);
        EXPECT_TRUE(ef.data_);
        EXPECT_EQ(ef.length_, payloadSize);

        // check data
        int segNo = 0;
        for (auto &d : packets)
        {
            if (d->getName()[-1].isSegment() && d->getName()[-2].toEscapedString() != NameComponents::Parity)
            {
                segNo = d->getName()[-1].toSegment();
                // iterate over the content
                EXPECT_EQ(memcmp(ef.data_+segNo*d->getContent().size(),
                                d->getContent()->data(), d->getContent().size()), 0)
                                << "segment " << segNo << " is invalid (data name " << d->getName() << endl;
            }
        }
    }
}

TEST(TestBitstreamData, TestLoadFromSlotRecovery)
{
    string basePrefix = "/my/base/prefix";
    Name stream = NameComponents::streamPrefix(basePrefix, "mystream");
    Name frame = Name(stream).appendSequenceNumber(0);
    int lifetime = 30;
    vector<shared_ptr<Data>> packets = getSampleFrameData(frame);
    auto expressRequests = [](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &r:requests)
        {
            p.timestampRequest(*r);
            p.setStatus(DataRequest::Status::Expressed, *r);
            p.triggerEvent(DataRequest::Status::Expressed, *r);
        }
    };
    auto replyData = [packets](vector<shared_ptr<DataRequest>> requests){
        UnitTestDataRequestProxy p;
        for (auto &d:packets)
        {
            for (auto &r:requests)
            {
                if (d->getName().match(r->getInterest()->getName()))
                {
                    p.timestampReply(*r);
                    p.setData(d, *r);
                    p.setStatus(DataRequest::Status::Data, *r);
                    p.triggerEvent(DataRequest::Status::Data, *r);
                }
            }
        }
    };
    vector<shared_ptr<DataRequest>> missing;
    auto onMissing = [&](IPipelineSlot* s, vector<shared_ptr<DataRequest>> requests){
        EXPECT_GT(requests.size(), 0);
        missing.insert(missing.end(), requests.begin(), requests.end());
        s->setRequests(requests);
        expressRequests(requests);
    };

    vector<shared_ptr<DataRequest>> requests;
    { // meta
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Meta), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // manifest
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Manifest), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // data
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }
    { // parity
        shared_ptr<Interest> i =
            make_shared<Interest>(Name(frame).append(NameComponents::Parity).appendSegment(0), lifetime);
        i->setMustBeFresh(false);
        requests.push_back(make_shared<DataRequest>(i));
    }

    int payloadSize = 0, paritySize = 0;
    int nData = 0, nParity = 0;
    int idx = 0;
    for (auto& d : packets)
    {
        if (d->getName()[-2].toEscapedString() != NameComponents::Parity)
        {
            if (d->getName()[-1].isSegment())
            {
                payloadSize += d->getContent().size();
                nData ++;
            }
        }
        else
        {
            paritySize += d->getContent().size();
            nParity ++;
        }
    }

    {   // TEST SLOT RECOVERY
        ASSERT_GT(nParity, 1);

        SlotCallbacksStub callbackStub;
        EXPECT_CALL(callbackStub, onMissing(_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Invoke(onMissing));

        shared_ptr<BufferSlot> slot = make_shared<BufferSlot>();

        slot->addOnNeedData(bind(&SlotCallbacksStub::onMissing, &callbackStub, _1, _2));

        slot->subscribe(PipelineSlotState::Pending,
                        bind(&SlotCallbacksStub::onPending, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Ready,
                        bind(&SlotCallbacksStub::onReady, &callbackStub, _1, _2));
        slot->subscribe(PipelineSlotState::Unfetchable,
                        bind(&SlotCallbacksStub::onUnfetchable, &callbackStub, _1, _2));
        slot->setRequests(requests);

        EXPECT_CALL(callbackStub, onPending(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onReady(_,_))
        .Times(1);
        EXPECT_CALL(callbackStub, onUnfetchable(_,_))
        .Times(0);

        // express initial requests
        expressRequests(requests);
        EXPECT_EQ(slot->getState(), PipelineSlotState::Pending);

        // reply initial data
        replyData(requests);
        EXPECT_GT(missing.size(), 0);

        // reply missing except one data segment so that FEC data is used to recover
        {
            UnitTestDataRequestProxy p;
            bool skipOne = true;
            for (auto &d:packets)
            {
                for (auto &r:missing)
                {
                    if (d->getName().match(r->getInterest()->getName()))
                    {
                        if (skipOne && d->getName()[-1].isSegment() &&
                            d->getName()[-2].toEscapedString() != NameComponents::Parity)
                            {
                                skipOne = false;
                                continue;
                            }

                        p.timestampReply(*r);
                        p.setData(d, *r);
                        p.setStatus(DataRequest::Status::Data, *r);
                        p.triggerEvent(DataRequest::Status::Data, *r);
                    }
                }
            }
        }

        EXPECT_EQ(slot->getState(), PipelineSlotState::Ready);

        // load data from slot
        DecodeQueueTesting::BitstreamDataTesting bsData_;

        bool recovered = false;
        shared_ptr<const BufferSlot> cSlot(slot);
        EncodedFrame ef = bsData_.loadFrom(cSlot, recovered);
        EXPECT_TRUE(recovered);
        EXPECT_TRUE(ef.data_);
        EXPECT_EQ(slot->getDataSegmentsNum(), nData);
        EXPECT_EQ(slot->getFetchedDataSegmentsNum(), nData-1);
        EXPECT_EQ(slot->getParitySegmentsNum(), nParity);
        EXPECT_EQ(slot->getFetchedParitySegmentsNum(), nParity);
        EXPECT_EQ(ef.length_, payloadSize);

        // check data
        int segNo = 0;
        for (auto &d : packets)
        {
            if (d->getName()[-1].isSegment() && d->getName()[-2] != NameComponents::Parity)
            {
                segNo = d->getName()[-1].toSegment();
                // iterate over the content
                EXPECT_EQ(memcmp(ef.data_+segNo*d->getContent().size(),
                                d->getContent()->data(), d->getContent().size()), 0)
                                << "segment " << segNo << " is invalid (data name " << d->getName() << endl;
            }
        }
    }
}

//******************************************************************************
int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
