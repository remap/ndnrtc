// 
// validator.cpp
//
//  Created by Peter Gusev on 05 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include "data-validator.hpp"

#include <ndn-cpp/data.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include "slot-buffer.hpp"

using namespace ndnrtc;
using namespace std;
using namespace ndn;

template<typename KeyChainType>
DataValidator<KeyChainType>::DataValidator(KeyChainType* keyChain, ISlotBuffer* buffer):
keyChain_(keyChain), buffer_(buffer)
{
    setDescription("data-validator");
}

template<typename KeyChainType>
DataValidator<KeyChainType>::~DataValidator(){
}

template<typename KeyChainType>
void DataValidator<KeyChainType>::validate(const boost::shared_ptr<ndn::Data>& data)
{
    LogDebugC << "verifying data " << data->getName().toUri() << std::endl;
    keyChain_->verifyData(data,
                          bind(&DataValidator::onVerifySuccess, this, _1),
                          (const OnDataValidationFailed)bind(&DataValidator::onVerifyFailure, this, _1, _2));

}

#pragma mark - private
template<typename KeyChainType>
void DataValidator<KeyChainType>::onVerifySuccess(const boost::shared_ptr<ndn::Data>& data)
{
    LogDebugC << "data verified: " << data->getName().toUri() << std::endl;
    markBufferData(data, true);
}

template<typename KeyChainType>
void DataValidator<KeyChainType>::onVerifyFailure(const boost::shared_ptr<ndn::Data>& data,
    const std::string& reason)
{
    LogDebugC << "failed to verify data() " << data->getName().toUri() << "): " 
        << reason << std::endl;
    markBufferData(data, false);
}

template<typename KeyChainType>
void DataValidator<KeyChainType>::markBufferData(const boost::shared_ptr<ndn::Data>& data, bool verified)
{
    buffer_->accessSlotExclusively(data->getName().toUri(),
			[verified, data](const boost::shared_ptr<ISlot>& slot){
				assert(slot.get());
				slot->markVerified(data->getName().toUri(), verified);
			},
			[this, data](){
				LogDebugC << "verification callback didn't find corresponding slot ("
					<< data->getName() << ")" << std::endl;
			});
}

// template instantiation for ndn::KeyChain
template class ndnrtc::DataValidator<KeyChain>;