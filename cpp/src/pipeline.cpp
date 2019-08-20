//
//  pipeline.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 13 March 2019.
//  Copyright 2019 Regents of the University of California
//

#include "pipeline.hpp"

#include <ndn-cpp/interest.hpp>

#include "frame-buffer.hpp"
#include "interest-queue.hpp"
#include "name-components.hpp"

using namespace std;
using namespace ndn;
using namespace ndnrtc;

Pipeline::Pipeline(std::shared_ptr<IInterestQueue> interestQ,
                   DispatchSlot dispatchSlot,
                   const ndn::Name &sequencePrefix, uint32_t nextSeqNo,
                   int step,
                   PrepareSegmentRequests requestsForFrame)
    : interestQ_(interestQ)
    , dispatchSlot_(dispatchSlot)
    , sequencePrefix_(sequencePrefix)
    , nextSeqNo_(nextSeqNo)
    , step_(step)
    , requestsForFrame_(requestsForFrame)
{
    description_ = "pipeline";
}

Pipeline::Pipeline(boost::asio::io_service &io, Face *f,
                   DispatchSlot dispatchSlot,
                   const Name &sequencePrefix,
                   uint32_t nextSeqNo, int step)
    : interestQ_(std::make_shared<RequestQueue>(io, f))
    , dispatchSlot_(dispatchSlot)
    , sequencePrefix_(sequencePrefix)
    , nextSeqNo_(nextSeqNo)
    , step_(step)
{
    description_ = "pipeline";
}

Pipeline::~Pipeline()
{

}

void
Pipeline::pulse()
{
    vector<std::shared_ptr<DataRequest>> frameRequests = requestsForFrame_(sequencePrefix_, nextSeqNo_);
    try {
        std::shared_ptr<IPipelineSlot> slot = dispatchSlot_();
        slot->setRequests(frameRequests);

        // TODO: decide on whether requesting missing segments should be here or not
        // Pipeline don't know whether FEC data need to be fetched
        // either provide flag for FEC data or don't request missing segments here
        // (let the jitter buffer do it?)
//        std::shared_ptr<IInterestQueue> interestQ = interestQ_;
//        slot->addOnNeedData([interestQ](IPipelineSlot *s, vector<std::shared_ptr<DataRequest>> &requests){
//            s->setRequests(requests);
//            interestQ->enqueueRequests(requests, std::make_shared<DeadlinePriority>(REQ_DL_PRI_RTX));
//        });

        LogDebugC << "dispatched slot " << slot->getPrefix()
                  << " " << frameRequests.size() << " requests total" << endl;

        interestQ_->enqueueRequests(frameRequests, std::make_shared<DeadlinePriority>(REQ_DL_PRI_DEFAULT));
        nextSeqNo_ += step_;
        pulseCount_++;
        onNewSlot(slot);
    } catch (runtime_error& e) {
        LogErrorC << "exception while trying to dispatch new slot: " << e.what() << endl;
    }
}

PrepareSegmentRequests
Pipeline::getPrepareSegmentRequests()
{
    return bind(&Pipeline::requestsForFrame, _1, _2,
        DEFAULT_LIFETIME, DEFAULT_NDATA_OUTSTANDING, DEFAULT_NFEC_OUTSTANDING);
}

vector<std::shared_ptr<DataRequest>>
Pipeline::requestsForFrame(const Name& sequencePrefix,
                           PacketNumber seqNo,
                           uint64_t lifetime,
                           size_t nDataOutstanding,
                           size_t nParityOutstanding)
{
    Name framePrefix = Name(sequencePrefix).appendSequenceNumber(seqNo);
    vector<std::shared_ptr<DataRequest>> requests;

    // meta
    {
        std::shared_ptr<Interest> i =
            std::make_shared<Interest>(Name(framePrefix).append(NameComponents::Meta), lifetime);

        i->setMustBeFresh(false);
        requests.push_back(std::make_shared<DataRequest>(i));
    }

    // manifest
    {
        std::shared_ptr<Interest> i =
            std::make_shared<Interest>(Name(framePrefix).append(NameComponents::Manifest), lifetime);

        i->setMustBeFresh(false);
        requests.push_back(std::make_shared<DataRequest>(i));
    }

    // data
    for (int seg = 0; seg < nDataOutstanding; ++seg)
    {
        std::shared_ptr<Interest> i =
            std::make_shared<Interest>(Name(framePrefix).appendSegment(seg), lifetime);

        i->setMustBeFresh(false);
        requests.push_back(std::make_shared<DataRequest>(i));
    }

    // parity
    for (int seg = 0; seg < nParityOutstanding; ++seg)
    {
        std::shared_ptr<Interest> i =
            std::make_shared<Interest>(Name(framePrefix).append(NameComponents::Parity)
                                                          .appendSegment(seg), lifetime);

        i->setMustBeFresh(false);
        requests.push_back(std::make_shared<DataRequest>(i));
    }

    return requests;
}
