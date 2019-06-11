//
//  rtx-controller.cpp
//  ndnrtc
//
//  Copyright 2017 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "rtx-controller.hpp"

#include "clock.hpp"
#include "drd-estimator.hpp"
#include "estimators.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

#define RTX_DEADLINE_MS 100

RetransmissionController::RetransmissionController(boost::shared_ptr<statistics::StatisticsStorage> storage,
                                                   boost::shared_ptr<IPlaybackQueue> playbackQueue,
                                                   const boost::shared_ptr<DrdEstimator> &drdEstimator)
    : StatObject(storage),
      playbackQueue_(playbackQueue),
      drdEstimator_(drdEstimator),
      enabled_(false)
{
    description_ = "rtx-controller";
}

void RetransmissionController::attach(IRtxObserver *observer)
{
    if (observer)
        observers_.push_back(observer);
}

void RetransmissionController::detach(IRtxObserver *observer)
{
    std::vector<IRtxObserver *>::iterator it = std::find(observers_.begin(), observers_.end(), observer);

    if (it != observers_.end())
        observers_.erase(it);
}

void RetransmissionController::setEnabled(bool enable)
{
    enabled_ = enable;
    LogTraceC << (enabled_ ? "enabled" : "disabled") << std::endl;
}

void RetransmissionController::onNewRequest(const boost::shared_ptr<BufferSlot> &slot)
{
    if (!enabled_)
        return;

    if (activeSlots_.find(slot->getPrefix()) != activeSlots_.end())
        throw std::runtime_error("slot is being tracked already");

    int64_t now = clock::millisecondTimestamp();
    int64_t queueSize = playbackQueue_->size() + playbackQueue_->pendingSize();
    // for key frames playback delay will be GOP milliseconds from now
    // NOTE: gop is assumed as 30 below. probably need to be changed to adequate number
    int64_t playbackDeadline = (slot->getNameInfo().class_ == SampleClass::Key ? now + playbackQueue_->samplePeriod() * 30 : now + queueSize);

    activeSlots_[slot->getPrefix()] = {slot, playbackDeadline};

    checkRetransmissions();
}

void RetransmissionController::onNewData(const BufferReceipt &receipt)
{
    if (enabled_)
        checkRetransmissions();
}

void RetransmissionController::onReset()
{
    activeSlots_.clear();
}

void RetransmissionController::checkRetransmissions()
{
    int64_t now = clock::millisecondTimestamp();

    for (auto it = activeSlots_.begin(); it != activeSlots_.end(); /* no increment */)
    {
        boost::shared_ptr<BufferSlot> slot = it->second.slot_;
        int64_t playbackDeadline = it->second.deadlineTimestamp_;
        double minDrd = fmin(drdEstimator_->getCachedEstimation(),drdEstimator_->getOriginalEstimation());
        bool needRtx = (playbackDeadline - now < minDrd);
        bool assembledOrCleared = (slot->getState() >= BufferSlot::State::Ready || slot->getState() == BufferSlot::State::Free);

        if (needRtx || assembledOrCleared)
        {
            activeSlots_.erase(it++);

            if (needRtx && !assembledOrCleared)
            {
                LogTraceC << "rtx required " << slot->dump()
                          << " playback in " << playbackDeadline - now << "ms" << std::endl;

                std::vector<boost::shared_ptr<const ndn::Interest>> pendingInterests = slot->getPendingInterests();
                if (pendingInterests.size())
                    for (auto o : observers_)
                        o->onRetransmissionRequired(pendingInterests);
            }
        }
        else
            ++it;
    }
}
