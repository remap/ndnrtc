// 
// ndnrtcOut.cpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcOut.hpp"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>
#include <cstring>
#include <OpenGL/gl3.h>

#include <ndnrtc/helpers/face-processor.hpp>
#include <ndnrtc/helpers/key-chain-manager.hpp>
#include <ndnrtc/local-stream.hpp>
#include <ndnrtc/statistics.hpp>

using namespace std;
using namespace std::placeholders;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

#define PAR_STREAM_NAME         "Streamname"
#define PAR_TARGET_BITRATE      "Targetbitrate"
#define PAR_ENCODE_WIDTH        "Encodewidth"
#define PAR_ENCODE_HEIGHT       "Encodeheight"
#define PAR_THREAD_NAME         "Threadname"
#define PAR_FEC                 "Fec"
#define PAR_SIGNING             "Signing"
#define PAR_FRAMEDROP           "Framedrop"
#define PAR_SEGSIZE             "Segmentsize"
#define PAR_FRESHNESS           "Freshness"
#define PAR_GOPSIZE             "Gopsize"

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
	return new ndnrtcOut(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (ndnrtcOut*)instance;
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
    PublishedFrame
};


/**
 * This maps output CHOP's channel names
 */
static std::map<InfoChopIndex, std::string> ChanNames = {
    { InfoChopIndex::PublishedFrame, "publishedFrame" }
};

//******************************************************************************
ndnrtcOut::ndnrtcOut(const OP_NodeInfo* info) :
ndnrtcTOPbase(info),
incomingFrameBuffer_(nullptr),
incomingFrameWidth_(0), incomingFrameHeight_(0)
{
    name_ = generateName("ndnrtcOut");
    statStorage_ = StatisticsStorage::createProducerStatistics();
    
    init();
    executeQueue_.push(bind(&ndnrtcOut::createLocalStream, this, _1, _2, _3));
}

ndnrtcOut::~ndnrtcOut()
{
}

void
ndnrtcOut::getGeneralInfo(TOP_GeneralInfo* ginfo)
{
	// Uncomment this line if you want the TOP to cook every frame even
	// if none of it's inputs/parameters are changing.
	ginfo->cookEveryFrame = false;
    ginfo->memPixelType = OP_CPUMemPixelType::BGRA8Fixed;
}

bool
ndnrtcOut::getOutputFormat(TOP_OutputFormat* format)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings
	return false;
}

void
ndnrtcOut::execute(const TOP_OutputFormatSpecs* outputFormat,
						OP_Inputs* inputs,
						TOP_Context *context)
{
    ndnrtcTOPbase::execute(outputFormat, inputs, context);
    
    int nInputs = inputs->getNumInputs();
    
    if (nInputs)
    {
        const OP_TOPInput* topInput = inputs->getInputTOP(0);
        
        if (topInput && stream_)
        {
            glBindTexture(GL_TEXTURE_2D, topInput->textureIndex);
            GetError()
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, incomingFrameBuffer_);
            GetError();
            
            // flip vertically and convert RGBA -> ARGB
            flipFrame(incomingFrameWidth_, incomingFrameHeight_, incomingFrameBuffer_,
                      false, true, true);
            
            boost::dynamic_pointer_cast<LocalVideoStream>(stream_)->incomingArgbFrame(topInput->width, topInput->height,
                                                                    incomingFrameBuffer_, incomingFrameBufferSize_);
        }
    }
}

int32_t
ndnrtcOut::getNumInfoCHOPChans()
{
	return (int32_t)ChanNames.size() + (int32_t)statStorage_->getIndicators().size();
}

void
ndnrtcOut::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan)
{
    InfoChopIndex idx = (InfoChopIndex)index;
    
    if (ChanNames.find(idx) != ChanNames.end())
    {
        switch (idx) {
            case InfoChopIndex::PublishedFrame:
            {
                chan->name = ChanNames[idx].c_str();
                chan->value = (float)publbishedFrame_;
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
ndnrtcOut::getInfoDATSize(OP_InfoDATSize* infoSize)
{
    return ndnrtcTOPbase::getInfoDATSize(infoSize);
}



void
ndnrtcOut::getInfoDATEntries(int32_t index,
                                 int32_t nEntries,
                                 OP_InfoDATEntries* entries)
{
    ndnrtcTOPbase::getInfoDATEntries(index, nEntries, entries);
}

void
ndnrtcOut::setupParameters(OP_ParameterManager* manager)
{
    ndnrtcTOPbase::setupParameters(manager);
    
    {
        OP_StringParameter streamName(PAR_STREAM_NAME);
        OP_NumericParameter targetBitrate(PAR_TARGET_BITRATE), encodeWidth(PAR_ENCODE_WIDTH),
                            encodeHeight(PAR_ENCODE_HEIGHT);
        
        streamName.label = "Stream Name";
        streamName.defaultValue = "video1";
        streamName.page = "Stream Config";
        
        targetBitrate.label = "Target Bitrate";
        targetBitrate.page = "Stream Config";
        targetBitrate.defaultValues[0] = 3000;
        targetBitrate.minSliders[0] = 500;
        targetBitrate.maxSliders[0] = 15000;
        
        encodeWidth.label = "Encode Width";
        encodeWidth.page = "Stream Config";
        encodeWidth.defaultValues[0] = 1280;
        encodeWidth.minSliders[0] = 320;
        encodeWidth.maxSliders[0] = 4096;
        
        encodeHeight.label = "Encode Height";
        encodeHeight.page = "Stream Config";
        encodeHeight.defaultValues[0] = 720;
        encodeHeight.minSliders[0] = 180;
        encodeHeight.maxSliders[0] = 2160;
        
        OP_ParAppendResult res = manager->appendString(streamName);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(targetBitrate);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(encodeWidth);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(encodeHeight);
        assert(res == OP_ParAppendResult::Success);
    }
    {
        OP_StringParameter threadName(PAR_THREAD_NAME);
        OP_NumericParameter fec(PAR_FEC), signing(PAR_SIGNING), frameDrop(PAR_FRAMEDROP),
                            segmentSize(PAR_SEGSIZE), freshness(PAR_FRESHNESS),
                            gopSize(PAR_GOPSIZE);
        
        threadName.label = "Thread Name";
        threadName.defaultValue = "t1";
        threadName.page = "Advanced";
        
        fec.label = "FEC";
        fec.page = "Advanced";
        fec.defaultValues[0] = 1.0;
        
        signing.label = "Signing";
        signing.page = "Advanced";
        signing.defaultValues[0] = 1.0;
        
        frameDrop.label = "Frame Drop";
        frameDrop.page = "Advanced";
        frameDrop.defaultValues[0] = 1.0;
        
        segmentSize.label = "Segment Size";
        segmentSize.page = "Advanced";
        segmentSize.defaultValues[0] = 8000;
        segmentSize.defaultValues[1] = 8000;
        segmentSize.minSliders[0] = 1000;
        segmentSize.maxSliders[0] = 8000;
        
        gopSize.label = "GOP Size";
        gopSize.page = "Advanced";
        gopSize.defaultValues[0] = 30;
        gopSize.minSliders[0] = 15;
        gopSize.maxSliders[0] = 60;
        
        freshness.label = "Freshness";
        freshness.page = "Advanced";
        freshness.defaultValues[0] = 2000;
        freshness.minSliders[0] = 1000;
        freshness.maxSliders[0] = 10000;
        
        OP_ParAppendResult res = manager->appendString(threadName);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(fec);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(signing);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(frameDrop);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(segmentSize);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(freshness);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(gopSize);
        assert(res == OP_ParAppendResult::Success);
    }
}

void
ndnrtcOut::pulsePressed(const char* name)
{
    ndnrtcTOPbase::pulsePressed(name);
    
	if (!strcmp(name, "Init"))
	{
        executeQueue_.push(bind(&ndnrtcOut::createLocalStream, this, _1, _2, _3));
	}
}

//******************************************************************************
#pragma mark private
void
ndnrtcOut::init()
{
    ndnrtcTOPbase::init();
    executeQueue_.push(bind(&ndnrtcOut::registerPrefix, this, _1, _2, _3));
    executeQueue_.push([this](const TOP_OutputFormatSpecs* outputFormat, OP_Inputs* inputs, TOP_Context *context)
    {
        if (keyChainManager_)
            keyChainManager_->publishCertificates();
    });
}

void
ndnrtcOut::checkInputs(const TOP_OutputFormatSpecs* outputFormat,
                       OP_Inputs* inputs,
                       TOP_Context *context)
{
    ndnrtcTOPbase::checkInputs(outputFormat, inputs, context);
    
    if (inputs->getNumInputs())
    {
        const OP_TOPInput *topInput = inputs->getInputTOP(0);
        if (topInput)
            if (topInput->width != incomingFrameWidth_ || topInput->height != incomingFrameHeight_)
                allocateIncomingFramebuffer(topInput->width, topInput->height);
    }
}

void
ndnrtcOut::createLocalStream(const TOP_OutputFormatSpecs *outputFormat,
                             OP_Inputs *inputs,
                             TOP_Context *context)
{
    if (!(faceProcessor_ && keyChainManager_))
        return;

    if (stream_)
        stream_.reset();
   
    MediaStreamSettings streamSettings(faceProcessor_->getIo(), readStreamParams(inputs));
   
    streamSettings.face_ = faceProcessor_->getFace().get();
    streamSettings.keyChain_ = keyChainManager_->instanceKeyChain().get();
    streamSettings.storagePath_ = ""; // TODO: try with storage
    streamSettings.sign_ = inputs->getParInt(PAR_SIGNING);
   
    stream_ = boost::make_shared<LocalVideoStream>(readBasePrefix(inputs),
                                                   streamSettings,
                                                   inputs->getParInt(PAR_FEC));
}

MediaStreamParams
ndnrtcOut::readStreamParams(OP_Inputs* inputs) const
{
    MediaStreamParams p;
    p.type_ = MediaStreamParams::MediaStreamTypeVideo;
    p.producerParams_.segmentSize_ = inputs->getParInt(PAR_SEGSIZE);
    p.producerParams_.freshness_ = {15, 30, 900};
    
    const char* streamName = (const char*)malloc(strlen(inputs->getParString(PAR_STREAM_NAME))+1);
    strcpy((char*)streamName, inputs->getParString(PAR_STREAM_NAME));
    p.streamName_ = std::string(streamName);
    free((void*)streamName);
    
    VideoCoderParams vcp;
    vcp.codecFrameRate_ = 30; // TODO: try 60fps;
    vcp.gop_ = inputs->getParInt(PAR_GOPSIZE);
    vcp.startBitrate_ = inputs->getParInt(PAR_TARGET_BITRATE);
    vcp.maxBitrate_ = inputs->getParInt(PAR_TARGET_BITRATE);
    vcp.encodeWidth_ = inputs->getParInt(PAR_ENCODE_WIDTH);
    vcp.encodeHeight_ = inputs->getParInt(PAR_ENCODE_HEIGHT);
    vcp.dropFramesOn_ =  inputs->getParInt(PAR_FRAMEDROP);
    
    const char *threadName = (const char*)malloc(strlen(inputs->getParString(PAR_THREAD_NAME))+1);
    strcpy((char*)threadName, inputs->getParString(PAR_THREAD_NAME));
    
    p.addMediaThread(VideoThreadParams(threadName, vcp));

    free((void*)threadName);
    
    LogInfoC << "creating stream with parameters: " << p << endl;
    
    return p;
}

void
ndnrtcOut::allocateIncomingFramebuffer(int w, int h)
{
    incomingFrameWidth_ = w; incomingFrameHeight_ = h;
    incomingFrameBufferSize_ = w*h*4*sizeof(unsigned char);
    incomingFrameBuffer_ = (unsigned char*)realloc(incomingFrameBuffer_, incomingFrameBufferSize_);
    memset(incomingFrameBuffer_, 0, incomingFrameBufferSize_);
}
