// 
// test-validator.cc
//
//  Created by Peter Gusev on 05 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include "gtest/gtest.h"
#include "src/data-validator.hpp"
#include "tests-helpers.hpp"
#include "mock-objects/slot-buffer-mock.hpp"
#include "mock-objects/ndn-cpp-mock.hpp"

// include .cpp in order to instantiate template with mock class
#include "src/data-validator.cpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace std;

void callOnVerifySuccess(const boost::shared_ptr<ndn::Data>& data, 
	const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed)
{
	onVerified(data);
}

TEST(TestValidator, TestCreate)
{
	MockNdnKeyChain keyChain;
	MockSlotBuffer slotBuffer;
	DataValidator<MockNdnKeyChain> dv(&keyChain, &slotBuffer);
}

TEST(TestValidator, TestVerifySuccess)
{
	boost::shared_ptr<MockSlot> slot(new MockSlot());
	MockNdnKeyChain keyChain;
	MockSlotBuffer slotBuffer;
	slotBuffer.slot_ = slot;

	DataValidator<MockNdnKeyChain> dv(&keyChain, &slotBuffer);
	std::string testDataName = "/test/data";
	boost::shared_ptr<ndn::Data> dataToVetify(new ndn::Data(testDataName));

	EXPECT_CALL(keyChain, verifyData(dataToVetify, _, _))
		.Times(1)
		.WillOnce(Invoke(&keyChain, &MockNdnKeyChain::callOnVerifySuccess));
	EXPECT_CALL(slotBuffer, accessSlotExclusively(testDataName,_,_))
		.Times(1)
		.WillOnce(Invoke(&slotBuffer, &MockSlotBuffer::callOnSlotAccess));
	EXPECT_CALL(*slot, markVerified(testDataName, true));

	dv.validate(dataToVetify);
}

TEST(TestValidator, TestVerifySuccessNotFound)
{
	MockNdnKeyChain keyChain;
	MockSlotBuffer slotBuffer;

	DataValidator<MockNdnKeyChain> dv(&keyChain, &slotBuffer);
	std::string testDataName = "/test/data";
	boost::shared_ptr<ndn::Data> dataToVetify(new ndn::Data(testDataName));

	EXPECT_CALL(keyChain, verifyData(dataToVetify, _, _))
		.Times(1)
		.WillOnce(Invoke(&keyChain, &MockNdnKeyChain::callOnVerifySuccess));
	EXPECT_CALL(slotBuffer, accessSlotExclusively(testDataName,_,_))
		.Times(1)
		.WillOnce(Invoke(&slotBuffer, &MockSlotBuffer::callOnSlotNotFound));

	dv.validate(dataToVetify);
}

TEST(TestValidator, TestVerifyFailure)
{
	boost::shared_ptr<MockSlot> slot(new MockSlot());
	MockNdnKeyChain keyChain;
	MockSlotBuffer slotBuffer;
	slotBuffer.slot_ = slot;

	DataValidator<MockNdnKeyChain> dv(&keyChain, &slotBuffer);
	std::string testDataName = "/test/data";
	boost::shared_ptr<ndn::Data> dataToVetify(new ndn::Data(testDataName));

	EXPECT_CALL(keyChain, verifyData(dataToVetify, _, _))
		.Times(1)
		.WillOnce(Invoke(&keyChain, &MockNdnKeyChain::callOnVerifyFailed));
	EXPECT_CALL(slotBuffer, accessSlotExclusively(testDataName,_,_))
		.Times(1)
		.WillOnce(Invoke(&slotBuffer, &MockSlotBuffer::callOnSlotAccess));
	EXPECT_CALL(*slot, markVerified(testDataName, false));

	dv.validate(dataToVetify);
}

TEST(TestValidator, TestVerifyFailureNotFound)
{
	MockNdnKeyChain keyChain;
	MockSlotBuffer slotBuffer;

	DataValidator<MockNdnKeyChain> dv(&keyChain, &slotBuffer);
	std::string testDataName = "/test/data";
	boost::shared_ptr<ndn::Data> dataToVetify(new ndn::Data(testDataName));

	EXPECT_CALL(keyChain, verifyData(dataToVetify, _, _))
		.Times(1)
		.WillOnce(Invoke(&keyChain, &MockNdnKeyChain::callOnVerifyFailed));
	EXPECT_CALL(slotBuffer, accessSlotExclusively(testDataName,_,_))
		.Times(1)
		.WillOnce(Invoke(&slotBuffer, &MockSlotBuffer::callOnSlotNotFound));

	dv.validate(dataToVetify);
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
