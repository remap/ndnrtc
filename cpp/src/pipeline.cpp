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

#define REQ_DL_PRI_DEFAULT  150
#define REQ_DL_PRI_RTX      50
#define REQ_DL_PRI_URGENT   0

using namespace std;
using namespace ndn;
using namespace ndnrtc;

Pipeline::Pipeline(boost::shared_ptr<IInterestQueue> interestQ,
                   DispatchSlot dispatchSlot,
                   const ndn::Name &sequencePrefix, uint32_t nextSeqNo,
                   int step)
    : interestQ_(interestQ)
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
    vector<boost::shared_ptr<DataRequest>> frameRequests = Pipeline::requestsForFrame(sequencePrefix_, nextSeqNo_);
    try {
        boost::shared_ptr<IPipelineSlot> slot = dispatchSlot_();
        slot->setRequests(frameRequests);
        
        LogDebugC << "dispatched slot " << slot->getPrefix()
                  << " " << frameRequests.size() << " requests total" << endl;
        
        interestQ_->enqueueRequests(frameRequests, boost::make_shared<DeadlinePriority>(REQ_DL_PRI_DEFAULT));
        nextSeqNo_ += step_;
        pulseCount_++;
        onNewSlot(slot);
    } catch (runtime_error& e) {
        LogErrorC << "exception while trying to dispatch new slot: " << e.what() << endl;
    }
}

vector<boost::shared_ptr<DataRequest>>
Pipeline::requestsForFrame(const Name& sequencePrefix,
                           PacketNumber seqNo,
                           uint64_t lifetime,
                           size_t nDataOutstanding,
                           size_t nParityOutstanding)
{
    Name framePrefix = Name(sequencePrefix).appendSequenceNumber(seqNo);
    vector<boost::shared_ptr<DataRequest>> requests;

    // meta
    {
        boost::shared_ptr<Interest> i =
            boost::make_shared<Interest>(Name(framePrefix).append(NameComponents::Meta), lifetime);
        
        i->setMustBeFresh(false);
        requests.push_back(boost::make_shared<DataRequest>(i));
    }
    
    // manifest
    {
        boost::shared_ptr<Interest> i =
            boost::make_shared<Interest>(Name(framePrefix).append(NameComponents::Manifest), lifetime);
        
        i->setMustBeFresh(false);
        requests.push_back(boost::make_shared<DataRequest>(i));
    }

    // data
    for (int seg = 0; seg < nDataOutstanding; ++seg)
    {
        boost::shared_ptr<Interest> i =
            boost::make_shared<Interest>(Name(framePrefix).appendSegment(seg), lifetime);
        
        i->setMustBeFresh(false);
        requests.push_back(boost::make_shared<DataRequest>(i));
    }
    
    // parity
    for (int seg = 0; seg < nParityOutstanding; ++seg)
    {
        boost::shared_ptr<Interest> i =
            boost::make_shared<Interest>(Name(framePrefix).append(NameComponents::Parity)
                                                          .appendSegment(seg), lifetime);
        
        i->setMustBeFresh(false);
        requests.push_back(boost::make_shared<DataRequest>(i));
    }
    
    return requests;
}
