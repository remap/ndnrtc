//
//  pipeline.hpp
//  ndnrtc
//
//  Created by Peter Gusev on 13 March 2019.
//  Copyright 2019 Regents of the University of California
//

#ifndef __pipeline_hpp__
#define __pipeline_hpp__

#include <stdlib.h>
#include <boost/signals2.hpp>
#include <ndn-cpp/name.hpp>
#include <boost/signals2.hpp>

#include "ndnrtc-common.hpp"
#include "ndnrtc-object.hpp"
#include "network-data.hpp"
#include "pool.hpp"

#define DEFAULT_LIFETIME 2000
#define DEFAULT_NDATA_OUTSTANDING 3
#define DEFAULT_NFEC_OUTSTANDING 1

namespace ndn {

class Interest;
class Data;
class Face;

}

namespace ndnrtc
{

class IInterestQueue;
class IPipelineSlot;
class FrameSlot;
class DataRequest;

typedef std::function<std::shared_ptr<IPipelineSlot>()> DispatchSlot;

typedef std::vector<std::shared_ptr<DataRequest>> DataRequestsArray;
typedef std::function<DataRequestsArray(const ndn::Name& framePrefix, PacketNumber seqNo)> PrepareSegmentRequests;

typedef std::function<void(const IPipelineSlot*, const DataRequest&)> OnSlotStateUpdate;
typedef boost::signals2::signal<void(const IPipelineSlot*, const DataRequest&)> SlotTrigger;
typedef boost::signals2::connection SlotTriggerConnection;


typedef std::function<void(IPipelineSlot*, std::vector<std::shared_ptr<DataRequest>>&)> OnNeedData;
typedef boost::signals2::signal<void(IPipelineSlot*, std::vector<std::shared_ptr<DataRequest>>&)> NeedDataTrigger;
typedef boost::signals2::connection NeedDataTriggerConnection;

enum class PipelineSlotState : int {
    Free = 1,
    New = 2,
    Pending = 3,
    Assembling = 4,
    Ready = 5,
    Unfetchable = 6
};

class IPipelineSlot {
public:
    virtual ~IPipelineSlot(){}

    virtual PipelineSlotState getState() const = 0;
//    virtual std::vector<std::shared_ptr<const DataRequest>> getAllRequests() const = 0;
//    virtual const std::vector<std::shared_ptr<const DataRequest>> getRequests(DataRequest::Status) const = 0;
    virtual const ndn::Name& getPrefix() const = 0;

    virtual void setRequests(const std::vector<std::shared_ptr<DataRequest>>&) = 0;
    virtual SlotTriggerConnection subscribe(PipelineSlotState, OnSlotStateUpdate) = 0;
    virtual NeedDataTriggerConnection addOnNeedData(OnNeedData) = 0;
};

class Pipeline : public NdnRtcComponent {
public:
    Pipeline(std::shared_ptr<IInterestQueue> interestQ,
             DispatchSlot dispatchSlot,
             const ndn::Name &sequencePrefix, uint32_t nextSeqNo,
             int step = 1,
             PrepareSegmentRequests = getPrepareSegmentRequests());

    Pipeline(boost::asio::io_service &io, ndn::Face *f,
             DispatchSlot dispatchSlot,
             const ndn::Name &sequencePrefix,
             uint32_t nextSeqNo, int step = 1);

    ~Pipeline();

    void pulse();
    boost::signals2::signal<void(std::shared_ptr<IPipelineSlot>)> onNewSlot;

    std::shared_ptr<IInterestQueue> getInterestQ() const { return interestQ_; }
    uint64_t getNextSeqNo() const { return nextSeqNo_; }

    static PrepareSegmentRequests getPrepareSegmentRequests();
    static std::vector<std::shared_ptr<DataRequest>>
        requestsForFrame(const ndn::Name& framePrefix,
                         PacketNumber seqNo,
                         uint64_t lifetime = DEFAULT_LIFETIME,
                         size_t nDataOustanding = DEFAULT_NDATA_OUTSTANDING,
                         size_t nParityOutstanding = DEFAULT_NFEC_OUTSTANDING);

private:
    uint64_t pulseCount_;
    DispatchSlot dispatchSlot_;
    PrepareSegmentRequests requestsForFrame_;
    std::shared_ptr<IInterestQueue> interestQ_;
    ndn::Name sequencePrefix_;
    int32_t step_;
    uint32_t nextSeqNo_;
};

}

#endif
