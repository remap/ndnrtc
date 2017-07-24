// 
// ndn-cpp-mock.hpp
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __ndn_cpp_mock_h__
#define __ndn_cpp_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

class MockNdnKeyChain 
{
public:
	MOCK_METHOD1(sign, void(ndn::Data&));
	MOCK_METHOD3(verifyData, void(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnDataValidationFailed& onVerifyFailed));

	void callOnVerifySuccess(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnDataValidationFailed& onVerifyFailed)

	{
		onVerified(data);
	}

	void callOnVerifyFailed(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnDataValidationFailed& onVerifyFailed)
	{
		onVerifyFailed(data, "");
	}
};

class MockNdnMemoryCache 
{
public:
	MOCK_METHOD2(setInterestFilter, void(const ndn::Name&, const ndn::OnInterestCallback&));
	MOCK_METHOD2(getPendingInterestsForName, void(const ndn::Name&, std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest> >&));
	MOCK_METHOD2(getPendingInterestsWithPrefix, void(const ndn::Name&, std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest> >&));
	MOCK_METHOD1(add, void(const ndn::Data&));
	MOCK_METHOD0(getStorePendingInterest, const ndn::OnInterestCallback&());

	void
	storePendingInterestCallback
	(const boost::shared_ptr<const ndn::Name>& prefix,
		const boost::shared_ptr<const ndn::Interest>& interest, ndn::Face& face,
		uint64_t interestFilterId,
		const boost::shared_ptr<const ndn::InterestFilter>& filter)
	{
	}
};

#endif
