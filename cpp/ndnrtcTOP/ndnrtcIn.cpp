//
//  ndnrtcIn.cpp
//  ndnrtcIn
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcIn.hpp"

#include <ndnrtc/statistics.hpp>

using namespace std;
using namespace std::placeholders;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

#define PAR_STREAM_NAME         "Streamname"
#define PAR_BASE_PREFIX         "Baseprefix"
#define PAR_LIFETIME            "Lifetime"
#define PAR_JITTER              "Jittersize"

#define GetError( )\
{\
for ( GLenum Error = glGetError( ); ( GL_NO_ERROR != Error ); Error = glGetError( ) )\
{\
switch ( Error )\
{\
case GL_INVALID_ENUM:      printf( "\n%s\n\n", "GL_INVALID_ENUM"      ); assert( 1 ); break;\
case GL_INVALID_VALUE:     printf( "\n%s\n\n", "GL_INVALID_VALUE"     ); assert( 1 ); break;\
case GL_INVALID_OPERATION: printf( "\n%s\n\n", "GL_INVALID_OPERATION" ); assert( 1 ); break;\
case GL_OUT_OF_MEMORY:     printf( "\n%s\n\n", "GL_OUT_OF_MEMORY"     ); assert( 1 ); break;\
default:                                                                              break;\
}\
}\
}

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

DLLEXPORT
TOP_PluginInfo
GetTOPPluginInfo(void)
{
    TOP_PluginInfo info;
    // This must always be set to this constant
    info.apiVersion = TOPCPlusPlusAPIVersion;
    
    // Change this to change the executeMode behavior of this plugin.
    info.executeMode = TOP_ExecuteMode::CPUMemWriteOnly;
    
    return info;
}

DLLEXPORT
TOP_CPlusPlusBase*
CreateTOPInstance(const OP_NodeInfo* info, TOP_Context* context)
{
    // Return a new instance of your class every time this is called.
    // It will be called once per TOP that is using the .dll
    return new ndnrtcIn(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
    // Delete the instance here, this will be called when
    // Touch is shutting down, when the TOP using that instance is deleted, or
    // if the TOP loads a different DLL
    delete (ndnrtcIn*)instance;
}

};

//******************************************************************************
static void NdnrtcStreamLoggingCallback(const char* message)
{
    std::cout << "[ndnrtc-stream] " << message << endl;
}

//******************************************************************************
/**
 * This enum identifies output DAT's different fields
 * The output DAT is a table that contains two
 * columns : name(identified by this enum) and value
 * (either float or string)
 */
enum class InfoChopIndex {
    ReceivedFrame,
    ReceivedFrameTimestamp,
    State
};


/**
 * This maps output CHOP's channel names
 */
static std::map<InfoChopIndex, std::string> ChanNames = {
    { InfoChopIndex::ReceivedFrame, "receivedFrame" },
    { InfoChopIndex::ReceivedFrameTimestamp, "receivedFrameTimestamp" },
    { InfoChopIndex::State, "state" }
};


//******************************************************************************
ndnrtcIn::ndnrtcIn(const OP_NodeInfo *info) :
ndnrtcTOPbase(info),
receivedFrameTimestamp_(0),
receivedFrame_(0),
bufferWrite_(false), bufferRead_(false),
frameBufferSize_(0), frameWidth_(0), frameHeight_(0),
frameBuffer_(nullptr)
{
    generateName("ndnrtcIn");
    statStorage_ = StatisticsStorage::createConsumerStatistics();
}

ndnrtcIn::~ndnrtcIn()
{
    
}

void
ndnrtcIn::getGeneralInfo(TOP_GeneralInfo *ginfo)
{
    ginfo->cookEveryFrame = true;
    ginfo->memPixelType = OP_CPUMemPixelType::BGRA8Fixed;
}

bool
ndnrtcIn::getOutputFormat(TOP_OutputFormat* format)
{
    format->width = frameWidth_;
    format->height = frameHeight_;
    return false;
}

void
ndnrtcIn::execute(const TOP_OutputFormatSpecs* outputFormat,
                            OP_Inputs* inputs,
                            TOP_Context* context)
{
    ndnrtcTOPbase::execute(outputFormat, inputs, context);
    
    if (!bufferWrite_)
    {
        bufferRead_ = true;
        
        // copy buffer to a texture or whatever
        
        bufferRead_ = false;
    }
}

int32_t
ndnrtcIn::getNumInfoCHOPChans()
{
    return (int32_t)ChanNames.size() + (int32_t)statStorage_->getIndicators().size();
}

void
ndnrtcIn::getInfoCHOPChan(int32_t index,
                          OP_InfoCHOPChan *chan)
{
    InfoChopIndex idx = (InfoChopIndex)index;
    
    if (ChanNames.find(idx) != ChanNames.end())
    {
        switch (idx) {
            case InfoChopIndex::ReceivedFrame:
                {
                    chan->name = ChanNames[idx].c_str();
                    chan->value = (float)receivedFrame_;
                }
                break;
            case InfoChopIndex::ReceivedFrameTimestamp:
                {
                    chan->name = ChanNames[idx].c_str();
                    chan->value = (float)receivedFrameTimestamp_;
                }
                break;
            case InfoChopIndex::State:
                {
                    chan->name = ChanNames[idx].c_str();
                    chan->value = 0;
                }
                break;
            default:
                break;
        }
    }
    else
    { // print statistics
        readStreamStats();
        
        int statIdx = ((int)idx - (int)ChanNames.size());
        int idx = 0;
        for (auto pair:statStorage_->getIndicators())
        {
            if (idx == statIdx)
            {
                chan->name = StatisticsStorage::IndicatorKeywords.at(pair.first).c_str();
                chan->value = (float)pair.second;
                break;
            }
            idx++;
        } // for
    } // else
}

bool
ndnrtcIn::getInfoDATSize(OP_InfoDATSize *infoSize)
{
    return ndnrtcTOPbase::getInfoDATSize(infoSize);
}

void
ndnrtcIn::getInfoDATEntries(int32_t index,
                            int32_t nEntries,
                            OP_InfoDATEntries *entries)
{
    ndnrtcTOPbase::getInfoDATEntries(index, nEntries, entries);
}

void
ndnrtcIn::setupParameters(OP_ParameterManager *manager)
{
    ndnrtcTOPbase::setupParameters(manager);
    
    {
        OP_StringParameter streamName(PAR_STREAM_NAME),
                           basePrefix(PAR_BASE_PREFIX);
        OP_NumericParameter reset("Reset");
        
        streamName.label = "Stream Name";
        streamName.defaultValue = "";
        streamName.page = "Stream Config";
        
        basePrefix.label= "Base Prefix";
        basePrefix.defaultValue = "";
        basePrefix.page = "Stream Config";
        
        reset.label = "Reset";
        reset.page = "Stream Config";
        
        OP_ParAppendResult res = manager->appendString(streamName);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendString(basePrefix);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendPulse(reset);
        assert(res == OP_ParAppendResult::Success);
    }
    {
        OP_NumericParameter jitterSize(PAR_JITTER), lifetime(PAR_LIFETIME);
        
        jitterSize.label = "Jitter Buffer Size (ms)";
        jitterSize.defaultValues[0] = 150;
        jitterSize.minSliders[0] = 150;
        jitterSize.maxSliders[0] = 2000;
        jitterSize.page = "Advanced";
        
        lifetime.label = "Interest lifetime (ms)";
        lifetime.defaultValues[0] = 2000;
        lifetime.minSliders[0] = 500;
        lifetime.maxSliders[0] = 10000;
        lifetime.page = "Advanced";
        
        OP_ParAppendResult res = manager->appendInt(jitterSize);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(lifetime);
        assert(res == OP_ParAppendResult::Success);
    }
}

void
ndnrtcIn::pulsePressed(const char *name)
{
    ndnrtcTOPbase::pulsePressed(name);
    
    if (!strcmp(name, "Reset"))
    {
        executeQueue_.push(bind(&ndnrtcIn::createRemoteStream, this, _1, _2, _3));
    }
}

void
ndnrtcIn::checkInputs(const TOP_OutputFormatSpecs* outputFormat,
                      OP_Inputs* inputs,
                      TOP_Context *context)
{
    ndnrtcTOPbase::checkInputs(outputFormat, inputs, context);
}

void
ndnrtcIn::createRemoteStream(const TOP_OutputFormatSpecs* outputFormat,
                             OP_Inputs* inputs,
                             TOP_Context *context)
{
    // check parameters
    if (ndnrtcInitialized_)
    {
        errorString_ = "";
        
        if (stream_)
            ndnrtc_destroyRemoteStream(stream_);
        
        RemoteStreamParams params = readStreamParams(inputs);
        
        if (!params.basePrefix || !params.streamName ||
            strlen(params.basePrefix) == 0 || strlen(params.streamName) == 0)
        {
            errorString_ = "Base prefix and stream name should be provided";
            return;
        }
        
        try {
            stream_ = ndnrtc_createRemoteStream(params, &NdnrtcStreamLoggingCallback);
        
            ((RemoteVideoStream*)stream_)->registerObserver(this);
            ((RemoteVideoStream*)stream_)->start("t1", this);
            
            if (!stream_)
                errorString_ = "Something went wrong while creating remote stream";
        }
        catch (exception &e)
        {
            errorString_ = string(e.what());
        }
        
        free((void*)params.basePrefix);
        free((void*)params.streamName);
    }
    else
        errorString_ = "Can't create stream: library must be initialized first";
}

RemoteStreamParams
ndnrtcIn::readStreamParams(OP_Inputs *inputs) const
{
    RemoteStreamParams p;
    
    p.basePrefix = (const char*)malloc(strlen(inputs->getParString(PAR_BASE_PREFIX))+1);
    strcpy((char*)p.basePrefix, inputs->getParString(PAR_BASE_PREFIX));
    p.streamName = (const char*)malloc(strlen(inputs->getParString(PAR_STREAM_NAME))+1);
    strcpy((char*)p.streamName, inputs->getParString(PAR_STREAM_NAME));
    
    p.lifetimeMs = inputs->getParInt(PAR_LIFETIME);
    p.jitterSizeMs = inputs->getParInt(PAR_JITTER);
    
    return p;
}

void
ndnrtcIn::allocateFramebuffer(int w, int h)
{
    frameWidth_ = w; frameHeight_ = h;
    frameBufferSize_ = w*h*4*sizeof(unsigned char);
    frameBuffer_ = (unsigned char*)realloc(frameBuffer_, frameBufferSize_);
    memset(frameBuffer_, 0, frameBufferSize_);
}

void
ndnrtcIn::onNewEvent(const ndnrtc::RemoteStream::Event &event)
{
    // handle new event
    cout << "new event " << event << endl;
}

uint8_t*
ndnrtcIn::getFrameBuffer(int width, int height)
{
    bufferWrite_ = true;
    
    if (frameWidth_ != width || frameHeight_ != height)
        allocateFramebuffer(width, height);
    return frameBuffer_;
}

void
ndnrtcIn::renderBGRAFrame(int64_t timestamp, uint frameNo,
                          int width, int height,
                          const uint8_t *buffer)
{
    receivedFrameTimestamp_ = timestamp;
    receivedFrame_ = frameNo;
    
    cout << "received frame " << frameNo << " at " << timestamp << endl;
    
    // let know that we have new frame ready for render
    bufferWrite_ = false;
}
