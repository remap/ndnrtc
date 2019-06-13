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

#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include <ndnrtc/helpers/face-processor.hpp>
#include <ndnrtc/helpers/key-chain-manager.hpp>
#include <ndnrtc/c-wrapper.h> // for ndnrtc_getVersion()
#include <ndnrtc/statistics.hpp>
#include <ndnrtc/stream.hpp>

#include "foundation-helpers.h"

using namespace std;
//using namespace std::placeholders;
using namespace ndnrtc;
using namespace ndnrtc::helpers;
using namespace ndnlog::new_api;

namespace ndn {
    class Face;
    class Interest;
}

#define PAR_NFD_HOST            "Nfdhost"
#define PAR_USE_MACOS_KEYCHAIN  "Usemacoskeychain"
#define PAR_INIT                "Init"
#define PAR_LOGFILE             "Logfile"
#define PAR_LOGLEVEL            "Loglevel"

//******************************************************************************
/**
 * This enum identifies output DAT's different fields
 * The output DAT is a table that contains two
 * columns: name (identified by this enum) and value
 * (either float or string)
 */
enum class InfoDatIndex {
    LibVersion,
    Prefix,
    NdnRtcPrefix,
    SignIdentity,
    SignCertificate,
    InstanceIdentity,
    InstanceCertificate
};

/**
 * This maps output DAT's field onto their textual representation
 * (table caption)
 */
static std::map<InfoDatIndex, std::string> RowNames = {
    { InfoDatIndex::LibVersion, "Library Version" },
    { InfoDatIndex::Prefix, "Prefix" },
    { InfoDatIndex::NdnRtcPrefix, "NDNRTC Prefix" },
    { InfoDatIndex::SignIdentity, "Sign Identity" },
    { InfoDatIndex::SignCertificate, "Sign Certificate" },
    { InfoDatIndex::InstanceIdentity, "Instance Identity" },
    { InfoDatIndex::InstanceCertificate, "Instance Certificate" }
};

const std::string ndnrtcTOPbase::SigningIdentityName = "/touchdesigner";
const std::string ndnrtcTOPbase::NdnRtcStreamName = "s";
const std::string ndnrtcTOPbase::NdnRtcTrheadName = "t";

static set<string> ReinitParams({PAR_USE_MACOS_KEYCHAIN, PAR_NFD_HOST});

ndnrtcTOPbase::ndnrtcTOPbase(const OP_NodeInfo* info) : nodeInfo_(info),
errorString_(""), warningString_(""),
statStorage_(nullptr),
stream_(nullptr),
opName_(extractOpName(info->opPath)),
reinitParams_(ReinitParams),
prefixRegistered_(0)
{
    description_ = "ndnrtcTOP_base";

    Logger::initAsyncLogging();
    logger_ = boost::make_shared<Logger>(ndnlog::NdnLoggerDetailLevelDefault,
                                         boost::make_shared<CallbackSink>(bind(&ndnrtcTOPbase::logSink, this, _1)));
}

ndnrtcTOPbase::~ndnrtcTOPbase()
{
    // making sure processing stopped
    if (faceProcessor_)
        faceProcessor_->stop();

    if (statStorage_)
        delete statStorage_;
}

void
ndnrtcTOPbase::getWarningString(OP_String *warning, void* reserved1)
{
    if (warningString_.size())
        warning->setString(warningString_.c_str());
}

void
ndnrtcTOPbase::getErrorString(OP_String *error, void* reserved1)
{
    if (errorString_.size())
        error->setString(errorString_.c_str());
}

void
ndnrtcTOPbase::execute(TOP_OutputFormatSpecs *outputFormat,
                       const OP_Inputs *inputs,
                       TOP_Context *context,
                       void* reserved1)
{
    bool reinit = false;
    std::string opName = extractOpName(nodeInfo_->opPath);
    
    if (opName_ != opName)
    {
        opName_ = opName;
        reinit = true;
    }
    
    set<string> updatedParams = checkInputs(outputFormat, inputs, context);
    
    // check if any of NDN's params were update so that we need to reinit NDN...
    if (ndnrtcTOPbase::intersect(updatedParams, ReinitParams).size() > 0)
        init();

    // check whether stream needs to be re-initialized
    if (ndnrtcTOPbase::intersect(updatedParams, reinitParams_).size() > 0)
        initStream();
    
    {
        try {
            while (executeQueue_.size())
            {
                executeQueue_.front()(outputFormat, inputs, context);
                executeQueue_.pop();
            }
        } catch (exception& e) {
            executeQueue_.pop(); // just throw this naughty block away
            logSink((string(string("Exception caught: ")+e.what())).c_str());
            errorString_ = e.what();
        }
    }
}

bool
ndnrtcTOPbase::getInfoDATSize(OP_InfoDATSize* infoSize, void *reserved1)
{
    infoSize->rows = (int)RowNames.size();
    infoSize->cols = 2;
    infoSize->byColumn = false;
    return true;
}

void
ndnrtcTOPbase::getInfoDATEntries(int32_t index,
                                 int32_t nEntries,
                                 OP_InfoDATEntries* entries,
                                 void *reserved1)
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
                const char *ndnrtcLibVersion = ndnrtc_getVersion();
                snprintf(tempBuffer2, strlen(ndnrtcLibVersion), "%s", ndnrtcLibVersion);
            }
                break;
            case InfoDatIndex::NdnRtcPrefix:
            {
                if (stream_)
                    strcpy(tempBuffer2, stream_->getPrefix().c_str());
            }
                break;
            case InfoDatIndex::Prefix:
            {
                if (stream_)
                    strcpy(tempBuffer2, stream_->getBasePrefix().c_str());
            }
                break;
            case InfoDatIndex::SignIdentity:
            {
                std::string identity(keyChainManager_->defaultKeyChain()->getDefaultIdentity().toUri().c_str());
                strcpy(tempBuffer2, identity.c_str());
            }
                break;
            case InfoDatIndex::SignCertificate:
            {
                strcpy(tempBuffer2, keyChainManager_->signingIdentityCertificate()->getName().toUri().c_str());
            }
                break;
            case InfoDatIndex::InstanceIdentity:
            {
                strcpy(tempBuffer2, keyChainManager_->instancePrefix().c_str());
            }
                break;
            case InfoDatIndex::InstanceCertificate:
            {
                strcpy(tempBuffer2, keyChainManager_->instanceCertificate()->getName().toUri().c_str());
            }
                break;
            default:
                break;
        }
        
        entries->values[0]->setString(tempBuffer1);
        entries->values[1]->setString(tempBuffer2);
    }
}

void
ndnrtcTOPbase::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
    {
        OP_StringParameter nfdHost(PAR_NFD_HOST);
        OP_NumericParameter useMacOsKeyChain(PAR_USE_MACOS_KEYCHAIN);
        
        nfdHost.label = "NFD Host";
        nfdHost.defaultValue = "localhost";
        nfdHost.page = "Lib Config";
        
        useMacOsKeyChain.label = "Use System KeyChain";
        useMacOsKeyChain.defaultValues[0] = 0;
        useMacOsKeyChain.page = "Lib Config";
        
        // TODO: setup list of system's identities
        
        OP_ParAppendResult res = manager->appendString(nfdHost);
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
    {
        OP_StringParameter sp;
        
        sp.name = PAR_LOGFILE;
        sp.label = "Log File";
        sp.page = "Advanced";
        logFile_ =("/tmp/"+this->opName_+".log");
        sp.defaultValue = logFile_.c_str();
        
        OP_ParAppendResult res = manager->appendString(sp);
        assert(res == OP_ParAppendResult::Success);
        
        #define NLEVELS 4
        static const char *logLevels[NLEVELS] = {"Info", "Debug", "All", "None"};
        
        sp.name = PAR_LOGLEVEL;
        sp.label = "Log Level";
        res = manager->appendMenu(sp, NLEVELS, logLevels, logLevels);
        assert(res == OP_ParAppendResult::Success);
    }
}

void
ndnrtcTOPbase::pulsePressed(const char* name, void *reserved1)
{
    if (!strcmp(name, "Init"))
    {
        init();
        initStream();
    }
}

set<string>
ndnrtcTOPbase::checkInputs(TOP_OutputFormatSpecs *, const OP_Inputs *inputs, TOP_Context *)
{
    assert(params_);
    set<string> updatedParams;
    
    // enable menu
//    inputs->enablePar(PAR_SIGNING_IDENTITY, !useMacOsKeyChain);
    
    if (params_->useMacOsKeyChain_ != inputs->getParInt(PAR_USE_MACOS_KEYCHAIN))
    {
        updatedParams.insert(PAR_USE_MACOS_KEYCHAIN);
        params_->useMacOsKeyChain_ = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    }
    
    if (params_->nfdHost_ != string(inputs->getParString(PAR_NFD_HOST)))
    {
        updatedParams.insert(PAR_NFD_HOST);
        params_->nfdHost_ = string(inputs->getParString(PAR_NFD_HOST));
    }
    
    string logFile = inputs->getParString(PAR_LOGFILE);
    string logLevel = inputs->getParString(PAR_LOGLEVEL);
    
    checkUpdateLogger(logFile, logLevel);
    
    return updatedParams;
}

set<string>
ndnrtcTOPbase::intersect(const std::set<std::string> &a, const std::set<std::string> &b)
{
    vector<string> x;
    set_intersection(a.begin(), a.end(), b.begin(), b.end(), back_inserter(x));
    return set<string>(make_move_iterator(x.begin()), make_move_iterator(x.end()));
}

//******************************************************************************
void
ndnrtcTOPbase::init()
{
    if (faceProcessor_)
        faceProcessor_->stop();

    stream_.reset();
    keyChainManager_.reset();
    faceProcessor_.reset();
    queue<ExecuteCallback>().swap(executeQueue_); // is it cool to clear the queue?

    executeQueue_.push(bind(&ndnrtcTOPbase::initFace, this, _1, _2, _3));
    executeQueue_.push(bind(&ndnrtcTOPbase::initKeyChainManager, this, _1, _2, _3));
}

void
ndnrtcTOPbase::initFace(TOP_OutputFormatSpecs* outputFormat,
                        const OP_Inputs* inputs,
                        TOP_Context *context)
{
    if (FaceProcessor::checkNfdConnection())
    {
        std::string hostname(inputs->getParString(PAR_NFD_HOST));
        faceProcessor_ = boost::make_shared<FaceProcessor>(hostname);
        faceProcessor_->start();
        
        if (!faceProcessor_->isProcessing())
        {
            errorString_ = "Failed to start face processor";
            faceProcessor_.reset();
            
            return;
        }
        
        LogInfoC << "initialized Face Processor" << std::endl;
    }
    else
    {
        errorString_ = "Can't connect to NFD";
    }
}

void
ndnrtcTOPbase::initKeyChainManager(TOP_OutputFormatSpecs* outputFormat,
                                   const OP_Inputs* inputs,
                                   TOP_Context *context)
{
    if (!faceProcessor_)
        return;

    std::string signingIdentity(ndnrtcTOPbase::SigningIdentityName);
    std::string instanceName(opName_);
    bool useMacOsKeyChain = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    
    string policyFilePath = string(get_resources_path())+"/policy.conf";
    string keyChainPath = string(get_resources_path())+"/keychain";
    
    try
    {
        errorString_ = "";
        boost::shared_ptr<ndn::KeyChain> kc = KeyChainManager::createKeyChain(keyChainPath);
        keyChainManager_ = boost::make_shared<KeyChainManager>(faceProcessor_->getFace(),
                                                               (useMacOsKeyChain ? KeyChainManager::createKeyChain("") : KeyChainManager::createKeyChain(keyChainPath)),
                                                               (useMacOsKeyChain ? signingIdentity : ndnrtcTOPbase::SigningIdentityName),
                                                               instanceName,
                                                               policyFilePath,
                                                               24*3600,
                                                               logger_);
        
        if (!useMacOsKeyChain)
        {
            // TODO: enable identities menu
//            inputs->enablePar(PAR_SIGNING_IDENTITY, false);
        }
        
//        LogInfoC << "command signing info: " << keyChainManager_->instanceKeyChain()->getDefaultCertificateName() << endl;
        
//        faceProcessor_->getFace()->setCommandSigningInfo(*keyChainManager_->instanceKeyChain(),
//                                                         keyChainManager_->instanceKeyChain()->getDefaultCertificateName());
    }
    catch(std::exception &e)
    {
        errorString_ = string("Error initializing library: ") + e.what();
    }
}

void
ndnrtcTOPbase::registerPrefix(TOP_OutputFormatSpecs* outputFormat,
                              const OP_Inputs* inputs,
                              TOP_Context *context)
{
    if (!(faceProcessor_ && keyChainManager_))
        return;

    ndn::Name prefix(readBasePrefix(inputs));

    prefixRegistered_ = 0;
    faceProcessor_->registerPrefix(prefix,
                                              [this](const boost::shared_ptr<const ndn::Name>& prefix,
                                                     const boost::shared_ptr<const ndn::Interest>& interest,
                                                     ndn::Face& face, uint64_t interestFilterId,
                                                     const boost::shared_ptr<const ndn::InterestFilter>& filter){
                                                  LogDebugC << "received interest " << interest->getName() << endl;
                                              },
                                              [this](const boost::shared_ptr<const ndn::Name>& prefix){
                                                  LogErrorC << "failed to register prefix " << prefix->toUri() << endl;
                                                  errorString_ = "Prefix registration failure: " + prefix->toUri();
                                              },
                                              [this](const boost::shared_ptr<const ndn::Name>& prefix,
                                                     uint64_t registeredPrefixId){
                                                  LogInfoC << "registered prefix " << *prefix << endl;
                                                  prefixRegistered_ = 1;
                                              });
}

std::string
ndnrtcTOPbase::extractOpName(std::string opPath) const
{
    size_t last = 0;
    size_t next = 0;

    while ((next = opPath.find("/", last)) != string::npos)
        last = next + 1;
    
    return opPath.substr(last);
}

void
ndnrtcTOPbase::readStreamStats()
{
    if (stream_)
        *statStorage_ = stream_->getStatistics();
}

string ndnrtcTOPbase::readBasePrefix(const OP_Inputs* inputs) const
{
    stringstream basePrefix;
    basePrefix << ndnrtcTOPbase::SigningIdentityName << "/" << opName_;
    return basePrefix.str();
}

void
ndnrtcTOPbase::logSink(const char *msg)
{
    cout << "[" << opName_ << "] " << msg;
}

void
ndnrtcTOPbase::flipFrame(int width, int height, uint8_t* buf,
                         bool flipH, bool flipV, bool convertToArgb)
{
    // this code flips image vertically (but can do it horizontally too) and
    // converts RGBA to ARGB (can be disabled too)
    // all this is done in-place and in one pass without additional copying
    
    int format = 4; // 4 bytes per pixel - ARGB32
    int stride = width * format;
    int yStop = (flipV) ? height / 2 : height;
    int xStop = (flipH && !(flipH && flipV)) ? width / 2 : width;
    
    for (int y = 0; y < yStop; ++y)
        for (int x = 0; x < xStop; ++x)
        {
            int xSwap = (flipH ? width - 1 - x : x);
            int ySwap = (flipV ? height - 1 - y : y);
            
            // swap ARGB pixels
            int p1Idx = y * stride + x * format;
            int p2Idx = ySwap * stride + xSwap * format;
            
            if (convertToArgb)
            {
                uint32_t temp = *(uint32_t*)(buf + p1Idx);
                *(uint32_t*)(buf + p1Idx) = (*(uint32_t*)(buf + p2Idx)) >> 24 | (*(uint32_t*)(buf + p2Idx)) << 8;
                *(uint32_t*)(buf + p2Idx) = temp >> 24 | temp << 8;
            }
            else
            {
                uint32_t temp = *(uint32_t*)(buf + p1Idx);
                *(uint32_t*)(buf + p1Idx) = *(uint32_t*)(buf + p2Idx);
                *(uint32_t*)(buf + p2Idx) = temp;
            }
        }
}

void
ndnrtcTOPbase::checkUpdateLogger(std::string logFile, std::string logLevel)
{
    if ((!streamLogger_ || logFile_ != logFile) && stream_)
    {
        logFile_ = logFile;
        boost::shared_ptr<ndnlog::new_api::Logger> logger = boost::make_shared<ndnlog::new_api::Logger>(ndnlog::NdnLoggerDetailLevelDefault, logFile_);
        stream_->setLogger(logger);
        streamLogger_ = logger;
    }
    
    static map<string, ndnlog::NdnLoggerDetailLevel> logLevels = {
        { "None", ndnlog::NdnLoggerDetailLevelNone },
        { "Info", ndnlog::NdnLoggerDetailLevelDefault },
        { "Debug", ndnlog::NdnLoggerDetailLevelDebug },
        { "All", ndnlog::NdnLoggerDetailLevelAll }
    };
    
    if (logLevels.find(logLevel) != logLevels.end())
    {
        ndnlog::NdnLoggerDetailLevel lvl = logLevels[logLevel];
        if (streamLogger_ && streamLogger_->getLogLevel() != lvl)
            streamLogger_->setLogLevel(lvl);
    }
}
