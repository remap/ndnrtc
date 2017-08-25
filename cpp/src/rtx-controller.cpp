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

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

RetransmissionController::RetransmissionController(boost::shared_ptr<statistics::StatisticsStorage> storage,
	boost::shared_ptr<IPlaybackQueue> playbackQueue):StatObject(storage), playbackQueue_(playbackQueue)
{
}

void RetransmissionController::attach(IRtxObserver* observer)
{
	if (observer)
		observers_.push_back(observer);
}

void RetransmissionController::detach(IRtxObserver* observer)
{
	std::vector<IRtxObserver*>::iterator it = std::find(observers_.begin(), observers_.end(), observer);

	if (it != observers_.end())
		observers_.erase(it);
}

void RetransmissionController::onNewRequest(const boost::shared_ptr<BufferSlot>& slot)
{
	if (activeSlots_.find(slot->getPrefix()) != activeSlots_.end())
		throw std::runtime_error("slot is being tracked already");

	int64_t playbackDeadline = clock::millisecondTimestamp()+playbackQueue_->size()+playbackQueue_->pendingSize();
	activeSlots_[slot->getPrefix()] = { slot, playbackDeadline };

	checkRetransmissions();
}

void RetransmissionController::onNewData(const BufferReceipt& receipt)
{
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

		if (playbackDeadline-now < 100)
		{
			activeSlots_.erase(it++);

			std::vector<boost::shared_ptr<const ndn::Interest>> pendingInterests = slot->getPendingInterests();
			if (pendingInterests.size())
				for (auto o:observers_)
					o->onRetransmissionRequired(pendingInterests);
		}
		else
			++it;
	}
}
