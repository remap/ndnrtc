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

namespace ndn {

class Interest;
class Data;

}

namespace ndnrtc
{

class IInterestQueue;
class IPipelineSlot;
class FrameSlot;
class DataRequest;

typedef std::function<boost::shared_ptr<IPipelineSlot>()> DispatchSlot;

typedef std::function<void(const IPipelineSlot*, const DataRequest&)> OnSlotStateUpdate;
typedef boost::signals2::signal<void(const IPipelineSlot*, const DataRequest&)> SlotTrigger;
typedef boost::signals2::connection SlotTriggerConnection;

enum class PipelineSlotState {
    Free,
    New,
    Pending,
    Assembling,
    Ready,
    Unfetchable
};
    
class IPipelineSlot {
public:
    virtual ~IPipelineSlot(){}
    
    virtual PipelineSlotState getState() const = 0;
//    virtual std::vector<boost::shared_ptr<const DataRequest>> getAllRequests() const = 0;
//    virtual const std::vector<boost::shared_ptr<const DataRequest>> getRequests(DataRequest::Status) const = 0;
    virtual const ndn::Name& getPrefix() const = 0;
    
    virtual void setRequests(const std::vector<boost::shared_ptr<DataRequest>>&) = 0;
    virtual SlotTriggerConnection subscribe(PipelineSlotState, OnSlotStateUpdate) = 0;
};
    
class Pipeline : public NdnRtcComponent {
public:
    Pipeline(boost::shared_ptr<IInterestQueue> interestQ,
             DispatchSlot dispatchSlot,
             const ndn::Name &sequencePrefix, uint32_t nextSeqNo,
             int step = 1);
    ~Pipeline();

    void pulse();
    boost::signals2::signal<void(boost::shared_ptr<IPipelineSlot>)> onNewSlot;
    
    boost::shared_ptr<IInterestQueue> getInterestQ() const { return interestQ_; }
    uint64_t getNextSeqNo() const { return nextSeqNo_; }
    
    static std::vector<boost::shared_ptr<DataRequest>>
        requestsForFrame(const ndn::Name& framePrefix,
                         PacketNumber seqNo,
                         uint64_t lifetime = 2000,
                         size_t nDataOustanding = 3,
                         size_t nParityOutstanding = 1);

private:
    uint64_t pulseCount_;
    DispatchSlot dispatchSlot_;
    boost::shared_ptr<IInterestQueue> interestQ_;
    ndn::Name sequencePrefix_;
    int32_t step_;
    uint32_t nextSeqNo_;
};
    
}

#endif
