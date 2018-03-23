//
//  ndnrtcTOP_base.cpp
//  ndnrtcTOP
//
//  Created by Peter Gusev on 2/16/18.
//  Copyright © 2018 UCLA REMAP. All rights reserved.
//

#include "ndnrtcTOP_base.hpp"

#include <stdio.h>
#include <map>
#include <iostream>

#include <ndnrtc/simple-log.hpp>
#include <ndnrtc/c-wrapper.h>
#include <ndnrtc/statistics.hpp>
#include <ndnrtc/helpers/key-chain-manager.hpp>
#include <ndnrtc/helpers/face-processor.hpp>

#include "foundation-helpers.h"

using namespace std;
using namespace std::placeholders;
using namespace ndnrtc;
using namespace ndnrtc::helpers;
using namespace ndnlog::new_api;

#define TOUCHDESIGNER_IDENTITY  "/touchdesigner"

#define PAR_NFD_HOST            "Nfdhost"
#define PAR_SIGNING_IDENTITY    "Signingidentity"
#define PAR_INSTANCE_NAME       "Instancename"
#define PAR_USE_MACOS_KEYCHAIN  "Usemacoskeychain"
#define PAR_INIT                "Init"

static map<const ndnrtcTOPbase*, std::string> TopNames;

//******************************************************************************
static void NdnrtcLibraryLoggingCallback(const char* message)
{
    cout << "[ndnrtc-library] " << message << endl;
}

//******************************************************************************
/**
 * This enum identifies output DAT's different fields
 * The output DAT is a table that contains two
 * columns: name (identified by this enum) and value
 * (either float or string)
 */
enum class InfoDatIndex {
    LibVersion,
    StreamPrefix,
    StreamName,
    StreamBasePrefix
};

/**
 * This maps output DAT's field onto their textual representation
 * (table caption)
 */
static std::map<InfoDatIndex, std::string> RowNames = {
    { InfoDatIndex::LibVersion, "Library Version" },
    { InfoDatIndex::StreamPrefix, "Stream Prefix" },
    { InfoDatIndex::StreamName, "Stream Name" },
    { InfoDatIndex::StreamBasePrefix, "Base Prefix" }
};

ndnrtcTOPbase::ndnrtcTOPbase(const OP_NodeInfo* info) : myNodeInfo(info),
errorString_(""), warningString_(""),
statStorage_(nullptr),
stream_(nullptr)
{
    Logger::initAsyncLogging();
    logger_ = boost::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelDefault,
                                         boost::make_shared<CallbackSink>(&NdnrtcLibraryLoggingCallback));
    logger_->log(ndnlog::NdnLoggerLevelInfo) << "Set up ndnrtc logging" << endl;
}

ndnrtcTOPbase::~ndnrtcTOPbase()
{
    deinitNdnrtcLibrary();
    TopNames.erase(this);
    if (statStorage_)
        delete statStorage_;
}

const char*
ndnrtcTOPbase::getWarningString()
{
    if (warningString_.size())
        return warningString_.c_str();
    return nullptr;
}

const char *
ndnrtcTOPbase::getErrorString()
{
    if (errorString_.size())
        return errorString_.c_str();
    return nullptr;
}

void
ndnrtcTOPbase::execute(const TOP_OutputFormatSpecs *outputFormat,
                       OP_Inputs *inputs,
                       TOP_Context *context)
{
    checkInputs(outputFormat, inputs, context);
    
    try {
        while (executeQueue_.size())
        {
            executeQueue_.front()(outputFormat, inputs, context);
            executeQueue_.pop();
        }
    } catch (exception& e) {
        executeQueue_.pop(); // just throw this naughty block away
        NdnrtcLibraryLoggingCallback((string(string("Exception caught: ")+e.what())).c_str());
        errorString_ = e.what();
    }
}

bool
ndnrtcTOPbase::getInfoDATSize(OP_InfoDATSize* infoSize)
{
    infoSize->rows = (int)RowNames.size();
    infoSize->cols = 2;
    infoSize->byColumn = false;
    return true;
}

void
ndnrtcTOPbase::getInfoDATEntries(int32_t index,
                             int32_t nEntries,
                             OP_InfoDATEntries* entries)
{
    // It's safe to use static buffers here because Touch will make it's own
    // copies of the strings immediately after this call returns
    // (so the buffers can be reuse for each column/row)
    static char tempBuffer1[4096];
    static char tempBuffer2[4096];
    memset(tempBuffer1, 0, 4096);
    memset(tempBuffer2, 0, 4096);
    
    InfoDatIndex idx = (InfoDatIndex)index;
    
    if (RowNames.find(idx) != RowNames.end())
    {
        strcpy(tempBuffer1, RowNames[idx].c_str());
        
        switch (idx) {
            case InfoDatIndex::LibVersion:
            {
                const char *ndnrtcLibVersion = ndnrtc_lib_version();
                snprintf(tempBuffer2, strlen(ndnrtcLibVersion), "%s", ndnrtcLibVersion);
            }
                break;
            case InfoDatIndex::StreamPrefix:
            {
                if (stream_)
                    strcpy(tempBuffer2, stream_->getPrefix().c_str());
            }
                break;
            case InfoDatIndex::StreamName:
            {
                if (stream_)
                    strcpy(tempBuffer2, stream_->getStreamName().c_str());
            }
                break;
            case InfoDatIndex::StreamBasePrefix:
            {
                if (stream_)
                    strcpy(tempBuffer2, stream_->getBasePrefix().c_str());
            }
                break;
            default:
                break;
        }
        
        entries->values[0] = tempBuffer1;
        entries->values[1] = tempBuffer2;
    }
}

void
ndnrtcTOPbase::setupParameters(OP_ParameterManager* manager)
{
    {
        OP_StringParameter nfdHost(PAR_NFD_HOST), signingIdentity(PAR_SIGNING_IDENTITY),
        instanceName(PAR_INSTANCE_NAME);
        OP_NumericParameter useMacOsKeyChain(PAR_USE_MACOS_KEYCHAIN);
        
        nfdHost.label = "NFD Host";
        nfdHost.defaultValue = "localhost";
        nfdHost.page = "Lib Config";
        
        signingIdentity.label = "Signing Identity";
        signingIdentity.defaultValue = TOUCHDESIGNER_IDENTITY;
        signingIdentity.page = "Lib Config";
        
        instanceName.label = "Instance name";
        instanceName.defaultValue = TopNames[this].c_str();
        instanceName.page = "Lib Config";
        
        useMacOsKeyChain.label = "Use System KeyChain";
        useMacOsKeyChain.defaultValues[0] = 0;
        useMacOsKeyChain.page = "Lib Config";
        
        OP_ParAppendResult res = manager->appendString(nfdHost);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendString(signingIdentity);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendString(instanceName);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(useMacOsKeyChain);
        assert(res == OP_ParAppendResult::Success);
    }
    {
        OP_NumericParameter    np;
        
        np.name = PAR_INIT;
        np.label = "Init";
        np.page = "Lib Config";
        
        OP_ParAppendResult res = manager->appendPulse(np);
        assert(res == OP_ParAppendResult::Success);
    }
}

void
ndnrtcTOPbase::pulsePressed(const char* name)
{
    if (!strcmp(name, "Init"))
    {
//        executeQueue_.push(bind(&ndnrtcTOPbase::initNdnrtcLibrary, this, _1, _2, _3));
        executeQueue_.push(bind(&ndnrtcTOPbase::initKeyChainManager, this, _1, _2, _3));
        executeQueue_.push(bind(&ndnrtcTOPbase::initFace, this, _1, _2, _3));
    }
}

void
ndnrtcTOPbase::libraryLog(const char *msg)
{
    NdnrtcLibraryLoggingCallback(msg);
}

void
ndnrtcTOPbase::checkInputs(const TOP_OutputFormatSpecs *, OP_Inputs *inputs, TOP_Context *)
{
    bool useMacOsKeyChain = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    inputs->enablePar(PAR_SIGNING_IDENTITY, !useMacOsKeyChain);
}

//******************************************************************************
void
ndnrtcTOPbase::initKeyChainManager(const TOP_OutputFormatSpecs* outputFormat,
                                   OP_Inputs* inputs,
                                   TOP_Context *context)
{
    std::string hostname(inputs->getParString(PAR_NFD_HOST));
    std::string signingIdentity(inputs->getParString(PAR_SIGNING_IDENTITY));
    std::string instanceName(inputs->getParString(PAR_INSTANCE_NAME));
    bool useMacOsKeyChain = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    
    string policyFilePath = string(get_resources_path())+"/policy.conf";
    string keyChainPath = string(get_resources_path())+"/keychain";
    
    try
    {
        errorString_ = "";
        
        faceProcessor_ = boost::make_shared<FaceProcessor>(hostname);
        faceProcessor_->start();
        
        keyChainManager_ = boost::make_shared<KeyChainManager>(faceProcessor_->getFace(),
                                                               (useMacOsKeyChain ? KeyChainManager::createKeyChain("") : KeyChainManager::createKeyChain(keyChainPath)),
                                                               (useMacOsKeyChain ? signingIdentity : TOUCHDESIGNER_IDENTITY),
                                                               instanceName,
                                                               policyFilePath,
                                                               24*3600,
                                                               logger_);
        
        if (!useMacOsKeyChain)
            inputs->enablePar(PAR_SIGNING_IDENTITY, false);
        
    }
    catch(std::exception &e)
    {
        errorString_ = string("Error initializing library: ") + e.what();
    }
    
    if (!ndnrtcInitialized_)
        errorString_ = "Failed to initialize library: check NFD is running";
}

void
ndnrtcTOPbase::initFace(const TOP_OutputFormatSpecs* outputFormat,
                        OP_Inputs* inputs,
                        TOP_Context *context)
{
    
}

void
ndnrtcTOPbase::initNdnrtcLibrary(const TOP_OutputFormatSpecs* outputFormat,
                             OP_Inputs* inputs,
                             TOP_Context *context)
{
    if (ndnrtcInitialized_)
        ndnrtc_deinit();
    
    std::string hostname(inputs->getParString(PAR_NFD_HOST));
    std::string signingIdentity(inputs->getParString(PAR_SIGNING_IDENTITY));
    std::string instanceName(inputs->getParString(PAR_INSTANCE_NAME));
    bool useMacOsKeyChain = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    
    try
    {
        errorString_ = "";
        
        if (useMacOsKeyChain)
            ndnrtcInitialized_ = ndnrtc_init(hostname.c_str(), nullptr, nullptr,
                                             signingIdentity.c_str(), instanceName.c_str(),
                                             &NdnrtcLibraryLoggingCallback);
        else if (get_resources_path())
        {
            inputs->enablePar(PAR_SIGNING_IDENTITY, false);
            
            string policyFilePath = string(get_resources_path())+"/policy.conf";
            string keyChainPath = string(get_resources_path())+"/keychain";
            ndnrtcInitialized_ = ndnrtc_init(hostname.c_str(), keyChainPath.c_str(), policyFilePath.c_str(),
                                             TOUCHDESIGNER_IDENTITY, instanceName.c_str(),
                                             &NdnrtcLibraryLoggingCallback);
        }
        else
        {
            errorString_ = "File-based keychain was not found";
        }
    }
    catch(std::exception &e)
    {
        errorString_ = string("Error initializing library: ") + e.what();
    }
    
    if (!ndnrtcInitialized_)
        errorString_ = "Failed to initialize library: check NFD is running";
}

void
ndnrtcTOPbase::deinitNdnrtcLibrary()
{
    ndnrtc_deinit();
}

std::string
ndnrtcTOPbase::generateName(string topBaseName) const
{
    // TODO make this function smarter, if needed
    int nTOPs = (int)TopNames.size();
    
    stringstream ss;
    ss << topBaseName << nTOPs;

    TopNames[this] = ss.str();
    
    return ss.str();
}

void
ndnrtcTOPbase::readStreamStats()
{
    if (stream_)
        *statStorage_ = stream_->getStatistics();
}

string ndnrtcTOPbase::readBasePrefix(OP_Inputs* inputs) const
{
    stringstream basePrefix;
    basePrefix << inputs->getParString(PAR_SIGNING_IDENTITY) << "/"
        << inputs->getParString(PAR_INSTANCE_NAME);
    return basePrefix.str();
}
