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

#include "ndnrtc-object.hpp"

namespace ndn {

class Interest;
class Data;

}

namespace ndnrtc
{

class IInterestQueue;
class FrameSlot;

class Pipeline : public NdnRtcComponent {
public:
    class ISlot {
    public:
        enum class State {
            Free,
            New,
            Assembling,
            Ready
        };

        virtual ~ISlot();

        virtual State getState() const = 0;
        virtual const std::vector<boost::shared_ptr<const ndn::Interest>> getOutstandingInterests() const = 0;
        virtual const ndn::Name& getPrefix() const = 0;

    };

    Pipeline(const ndn::Name &sequencePrefix, uint32_t nextSeqNo);
    ~Pipeline();

    void pulse();

    boost::signals2::signal<void(boost::shared_ptr<ISlot>)> onNewSlot;

private:
    boost::shared_ptr<IInterestQueue> interestQ_;
    ndn::Name sequencePrefix_;
    int32_t step_;
    uint32_t nextSeqNo_;
};

}

#endif
