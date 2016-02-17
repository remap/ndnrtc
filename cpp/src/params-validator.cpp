// 
//  params-validator.cpp
//  ndnrtc
//
//  Copyright 2015 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "params-validator.h"

using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace std;



typedef boost::error_info<struct tag_msg, string> errmsg_info;
typedef boost::error_info<struct tag_param_name, string> errmsg_param_name;
typedef boost::error_info<struct tag_param, string> errmsg_param_value;
typedef boost::error_info<struct tag_param, int> errmsg_param;

const unsigned int ParamsValidator::MaxUserNameLen = 64;
const unsigned int ParamsValidator::MaxStreamNameLen = 64;
const unsigned int ParamsValidator::MaxThreadNameLen = 64;

//******************************************************************************
void ParamsValidator::validateParams(const MediaThreadParams& params)
{
    try {
        validateName(params.threadName_, MaxUserNameLen);
    } catch (param_validation_exception& e) {
        e << errmsg_param_name{"thread name"};
        throw;
    }
}

void ParamsValidator::validateParams(const VideoCoderParams& params)
{
}

void ParamsValidator::validateParams(const VideoThreadParams& params)
{
    validateParams(params.coderParams_);
}

void ParamsValidator::validateParams(const GeneralProducerParams& params)
{
    if (params.segmentSize_ == 0)
        BOOST_THROW_EXCEPTION(param_validation_exception("segment size can't be zero"));
}

void ParamsValidator::validateParams(const MediaStreamParams& params)
{
    validateParams(params.producerParams_);
    validateName(params.streamName_, MaxStreamNameLen);
    
    if (params.synchronizedStreamName_.size() > MaxStreamNameLen)
        BOOST_THROW_EXCEPTION(param_validation_exception("stream name is too long") << errmsg_param_name{"synchronizedStreamName"});
    
    for (auto thread:params.mediaThreads_)
    {
        if (params.type_ == MediaStreamParams::MediaStreamTypeAudio)
            validateParams(*thread);
        else
            validateParams(*((VideoThreadParams*)thread));
    }
}

void ParamsValidator::validateParams(const GeneralConsumerParams& params)
{
    if (params.bufferSlotsNum_ == 0)
        BOOST_THROW_EXCEPTION(param_validation_exception("number of buffer slots can't be zero"));
    
    if (params.slotSize_ == 0)
        BOOST_THROW_EXCEPTION(param_validation_exception("buffer slot size can't be zero"));
}

void ParamsValidator::validateParams(const HeadlessModeParams& params)
{
    validateName(params.username_, MaxUserNameLen);
}

void ParamsValidator::validateParams(const GeneralParams& params)
{
}

void ParamsValidator::validateParams(const AppParams& params)
{
    validateParams(params.generalParams_);
    validateParams(params.headlessModeParams_);
    validateParams(params.audioConsumerParams_);
    validateParams(params.videoConsumerParams_);
    
    for (auto stream:params.defaultAudioStreams_)
        validateParams(*stream);
    for (auto stream:params.defaultVideoStreams_)
        validateParams(*stream);
}

void ParamsValidator::validateName(const std::string& name, const int maxLength)
{
    if (name.size() == 0)
        BOOST_THROW_EXCEPTION(param_validation_exception("name parameter is invalid") << errmsg_param_value{name});
    if (name.size() > maxLength)
        BOOST_THROW_EXCEPTION(param_validation_exception("name parameter is too long") << errmsg_param_value{name});
}
