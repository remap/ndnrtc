// 
// buffer-control.cpp
//
//  Created by Peter Gusev on 07 June 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "buffer-control.h"
#include "name-components.h"
#include "drd-estimator.h"
#include "frame-data.h"
#include "statistics.h"

using namespace ndnrtc;
using namespace ndnrtc::statistics;

BufferControl::BufferControl(const boost::shared_ptr<DrdEstimator>& drdEstimator,
    const boost::shared_ptr<IBuffer>& buffer,
    const boost::shared_ptr<statistics::StatisticsStorage>& storage):
drdEstimator_(drdEstimator), buffer_(buffer), sstorage_(storage)
{
	description_ = "buffer-control";
}

BufferControl::~BufferControl()
{
    std::cout << "BufferControl::~BufferControl()" << std::endl;
}

void 
BufferControl::attach(IBufferControlObserver* o)
{
	if (o) observers_.push_back(o);
}

void 
BufferControl::detach(IBufferControlObserver* o)
{
    std::vector<IBufferControlObserver*>::iterator it  = std::find(observers_.begin(), observers_.end(), o);
    if (it != observers_.end()) observers_.erase(it);
}

void
BufferControl::segmentArrived(const boost::shared_ptr<WireSegment>& segment)
{
	if (buffer_->isRequested(segment))
	{
		BufferReceipt receipt = buffer_->received(segment);
        std::cout << "DRD " << receipt.segment_->getDrdUsec()/1000 << std::endl;
		drdEstimator_->newValue(receipt.segment_->getDrdUsec()/1000, receipt.segment_->isOriginal());
        
        (*sstorage_)[Indicator::DrdOriginalEstimation] = drdEstimator_->getOriginalEstimation();
        (*sstorage_)[Indicator::DrdCachedEstimation] = drdEstimator_->getCachedEstimation();

		if (segment->isPacketHeaderSegment())
        {
            double rate = receipt.segment_->getData()->packetHeader().sampleRate_;
			for (auto& o:observers_) o->targetRateUpdate(rate);
            
            (*sstorage_)[Indicator::CurrentProducerFramerate] = rate;
        }

		if (receipt.slot_->getFetchedNum() == 1)
			for (auto& o:observers_) o->sampleArrived(segment->getPlaybackNo());

		LogDebugC << "added segment " << receipt.segment_->getInfo().getSuffix(suffix_filter::Thread)
			<< (receipt.segment_->isOriginal() ? " ORIG" : " CACH")
            << std::endl;
	}
	else
		LogWarnC << "received data is not in the buffer: " << segment->getData()->getName() << std::endl;
}
