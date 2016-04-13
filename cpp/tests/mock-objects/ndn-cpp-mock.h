// 
// ndn-cpp-mock.h
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#ifndef __ndn_cpp_mock_h__
#define __ndn_cpp_mock_h__

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ndn-cpp/security/key-chain.hpp>

class MockNdnKeyChain 
{
public:
	MOCK_METHOD3(verifyData, void(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed));

	void callOnVerifySuccess(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed)

	{
		onVerified(data);
	}

	void callOnVerifyFailed(const boost::shared_ptr<ndn::Data>& data, 
		const ndn::OnVerified& onVerified, const ndn::OnVerifyFailed& onVerifyFailed)
	{
		onVerifyFailed(data);
	}
};

#endif
