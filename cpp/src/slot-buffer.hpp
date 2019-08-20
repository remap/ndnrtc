// 
// slot-buffer.hpp
//
//  Created by Peter Gusev on 05 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __slot_buffer_h__
#define __slot_buffer_h__

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

namespace ndnrtc {
	class ISlot {
	public:
		virtual void markVerified(const std::string& segmentName, bool verified) = 0;
	};

	typedef std::function<void(std::shared_ptr<ISlot> slot)> OnSlotAccess;
	typedef std::function<void()> OnNotFound;

	class ISlotBuffer {
	public:
		virtual void accessSlotExclusively(const std::string& slotName,
			const OnSlotAccess& onSlotAccess,
			const OnNotFound& onNotFound) = 0;
	};
};

#endif