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

using namespace ndnrtc;

BufferControl::BufferControl(const boost::shared_ptr<DrdEstimator>& drdEstimator, 
	const boost::shared_ptr<Buffer>& buffer, 
	const boost::shared_ptr<IBufferLatencyControl>& latencyControl):
drdEstimator_(drdEstimator), buffer_(buffer),
latencyControl_(latencyControl)
{
	description_ = "buffer-control";
}

BufferControl::~BufferControl()
{}

void
BufferControl::segmentArrived(const boost::shared_ptr<WireSegment>& segment)
{
	if (buffer_->isRequested(segment))
	{
		Buffer::Receipt receipt = buffer_->received(segment);
		drdEstimator_->newValue(receipt.segment_->getRoundTripDelayUsec()/1000, receipt.segment_->isOriginal());
		informLatencyControl(receipt);

		LogTraceC << "added segment " << receipt.segment_->getInfo().getSuffix(suffix_filter::Stream) 
			<< (receipt.segment_->isOriginal() ? "ORIG" : "CACH")
			<< ". DRD: original - " << drdEstimator_->getOriginalEstimation() 
			<< " cached - " << drdEstimator_->getCachedEstimation() << std::endl;
	}
	else
		LogWarnC << "received unwanted/removed(late) data " << segment->getData()->getName() << std::endl;
}

void 
BufferControl::informLatencyControl(const Buffer::Receipt& receipt)
{
	if (!receipt.segment_->getInfo().isParity_ && 
		receipt.segment_->getInfo().segNo_ == 0)
		latencyControl_->setTargetRate(receipt.segment_->getData()->packetHeader().sampleRate_);
	
	if (receipt.slot_->getFetchedNum() == 1)
		latencyControl_->sampleArrived();
}