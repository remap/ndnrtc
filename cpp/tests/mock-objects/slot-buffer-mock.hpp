// 
// slot-buffer-mock.hpp
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __slot_buffer_mock_h__
#define __slot_buffer_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/slot-buffer.hpp"

class MockSlotBuffer : public ndnrtc::ISlotBuffer {
public:
	MOCK_METHOD3(accessSlotExclusively, void(const std::string&,
		const ndnrtc::OnSlotAccess&, const ndnrtc::OnNotFound&));

	void callOnSlotAccess(const std::string&,
		const ndnrtc::OnSlotAccess& onSlotAccess, const ndnrtc::OnNotFound&)
	{
		onSlotAccess(slot_);
	}

	void callOnSlotNotFound(const std::string&,
		const ndnrtc::OnSlotAccess&, const ndnrtc::OnNotFound& onNotFound)
	{
		onNotFound();
	}

	boost::shared_ptr<ndnrtc::ISlot> slot_;
};

class MockSlot : public ndnrtc::ISlot {
public:
	MOCK_METHOD2(markVerified, void(const std::string&, bool));
};

#endif
