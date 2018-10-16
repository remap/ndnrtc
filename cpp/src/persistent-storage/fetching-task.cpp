//
// fetching-task.cpp
//
//  Created by Peter Gusev on 27 May 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "fetching-task.hpp"
#include "storage-engine.hpp"
#include "frame-buffer.hpp"
#include "frame-data.hpp"

#include <ndn-cpp/interest.hpp>

using namespace ndnrtc;
using namespace ndn;
using namespace boost;

#define NSEGMENTS_GUESS_DELTA 5
#define NSEGMENTS_GUESS_KEY 30

FrameFetchingTask::FrameFetchingTask(const ndn::Name& frameName,
                          const shared_ptr<IFetchMethod>& fetchMethod,
                          OnFetchingComplete onFetchingComplete,
                          OnFetchingFailed onFetchingFailed,
                          const FetchingTask::Settings& settings,
                          OnSegment onFirstSegment, 
                          OnSegment onZeroSegment):
    state_(Created),
    fetchMethod_(fetchMethod),
    onFetchingComplete_(onFetchingComplete), onFetchingFailed_(onFetchingFailed),
    settings_(settings),
    onFirstSegment_(onFirstSegment), onZeroSegment_(onZeroSegment)
{
    try {

        if (!NameComponents::extractInfo(frameName, frameNameInfo_))
            throw std::runtime_error("Couldn't extract namespace info from frame name "+
                frameName.toUri());
    }
    catch (std::exception& e)
    {
        throw std::runtime_error("Couldn't extract namespace info from frame name "+
            frameName.toUri()+": "+e.what());
    }

    description_ = "fetch-task-" + frameNameInfo_.getSuffix(suffix_filter::Sample).toUri();
    slot_ = make_shared<BufferSlot>();
}

FrameFetchingTask::~FrameFetchingTask()
{
    if (isFetching())
        cancel();
}


void FrameFetchingTask::start()
{
    std::vector<shared_ptr<const Interest>> interests = prepareBatch(frameNameInfo_.getPrefix(prefix_filter::Sample));
    taskProgress_ = 0;
    taskCompletion_ = interests.size() * (1+settings_.nRtx_);
    nNacks_ = 0;
    nTimeouts_ = 0;
    state_ = Fetching;

    LogDebugC << "start fetching task. completion " << taskCompletion_ << std::endl;

    slot_->segmentsRequested(interests);
    for (auto i:interests)
    {
        if (state_ == Fetching)
            requestSegment(i);
    }
}

void FrameFetchingTask::cancel()
{
    state_ = Canceled;
}

void FrameFetchingTask::requestSegment(const shared_ptr<const Interest>& interest)
{
    shared_ptr<FrameFetchingTask> self = shared_from_this();
    LogTraceC << "requesting segment " << interest->getName() << std::endl;

    taskProgress_ += 1;
    fetchMethod_->express(interest, 
        [self, this](const shared_ptr<const Interest>& interest, const shared_ptr<Data>& data)
        {
            if (data->getMetaInfo().getType() != ndn_ContentType_NACK)
            {
                NamespaceInfo info;
                NameComponents::extractInfo(data->getName(), info);
                boost::shared_ptr<WireSegment> segment = WireSegment::createSegment(info, data, interest);
                BufferSlot::State s = slot_->getState();
                shared_ptr<SlotSegment> seg = slot_->segmentReceived(segment);
                taskProgress_ += (settings_.nRtx_+1) - seg->getRequestNum();

                LogTraceC << "received " << data->getName() 
                          << ". progress " << taskProgress_ << "/" << taskCompletion_ << std::endl;

                if (s == BufferSlot::New && slot_->getState() >= BufferSlot::New)
                    if (onFirstSegment_) onFirstSegment_(self, seg);
                if (seg->getInfo().segNo_ == 0)
                    if (onZeroSegment_) onZeroSegment_(self, seg);

                checkMissingSegments();
                checkCompletion();
            }
        }, 
        [self, this](const shared_ptr<const Interest>& interest) // onTimeout
        {
            LogTraceC << "timeout for " << interest->getName() << std::endl;

            nTimeouts_++;
            if (state_ == Fetching)
                if (slot_->getRtxNum(interest->getName()) < settings_.nRtx_)
                    requestSegment(interest);
            checkCompletion();
        }, 
        [self, this](const shared_ptr<const Interest>& interest, const shared_ptr<NetworkNack>& networkNack)
        {
            LogTraceC << "NACK for " << interest->getName() << std::endl;

            nNacks_++;
            taskProgress_ += (settings_.nRtx_ - slot_->getRtxNum(interest->getName()));
            checkCompletion();
        });
}

std::vector<shared_ptr<const Interest>> 
FrameFetchingTask::prepareBatch(ndn::Name n, bool noParity) const
{
    std::vector<boost::shared_ptr<const Interest>> interests;
    unsigned int nData = (frameNameInfo_.isDelta_ ? NSEGMENTS_GUESS_DELTA : NSEGMENTS_GUESS_KEY);
    unsigned int nParity = (unsigned int)round(0.2*nData);

    for (int segNo = 0; segNo < nData; ++segNo)
    {
        Name iname(n);
        iname.appendSegment(segNo);
        interests.push_back(makeInterest(iname));
    }

    if (!noParity)
    {
        n.append(NameComponents::NameComponentParity);
        for (int segNo = 0; segNo < nParity; ++segNo)
        {
            Name iname(n);
            iname.appendSegment(segNo);
            interests.push_back(makeInterest(iname));
        }
    }

    return interests;
}

void
FrameFetchingTask::checkMissingSegments()
{
    std::vector<boost::shared_ptr<const Interest>> interests;
    for (auto& n:slot_->getMissingSegments())
    {
        taskCompletion_ += (1+settings_.nRtx_);
        interests.push_back(makeInterest(n));
    }

    if (interests.size())
    {
        slot_->segmentsRequested(interests);
        for (auto i:interests)
            requestSegment(i);
    }
}

void
FrameFetchingTask::checkCompletion()
{
    if (slot_->getState() == BufferSlot::Ready)
    {
        if (state_ == Fetching)
        {
            state_ = Completed;
            onFetchingComplete_(shared_from_this(), slot_);
        }
    }
    else if (taskProgress_ >= taskCompletion_)
    {
        if (state_ == Fetching)
        {
            state_ = Failed;
            std::stringstream ss;
            ss << "Couldn't fetch frame: " << nTimeouts_ << " timeouts, " << nNacks_ << " NACKs.";
            onFetchingFailed_(shared_from_this(), ss.str());
        }
    }
}

boost::shared_ptr<const ndn::Interest> 
FrameFetchingTask::makeInterest(const ndn::Name& name) const
{
    boost::shared_ptr<Interest> i = boost::make_shared<Interest>(name, settings_.interestLifeTimeMs_);
    i->setMustBeFresh(false);

    return i;
}

//******************************************************************************
void
FetchMethodLocal::express(const boost::shared_ptr<const ndn::Interest>& interest,
                          ndn::OnData onData,
                          ndn::OnTimeout,
                          ndn::OnNetworkNack onNack)
{
    shared_ptr<Data> data = storage_->get(interest->getName());
    if (data.get())
        onData(interest, data);
    else
        onNack(interest, make_shared<NetworkNack>());
}

void 
FetchMethodRemote::express(const boost::shared_ptr<const ndn::Interest>& interest,
                     ndn::OnData onData,
                     ndn::OnTimeout onTimeout,
                     ndn::OnNetworkNack onNack)
{
    face_->expressInterest(*interest, onData, onTimeout, onNack);
}