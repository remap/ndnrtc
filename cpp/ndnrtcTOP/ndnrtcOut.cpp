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

#define TOUCHDESIGNER_IDENTITY  "/touchdesigner"

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
    generateName("ndnrtcOut");
    statStorage_ = StatisticsStorage::createProducerStatistics();
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
            
            // convert RGBA -> ARGB
            for (int i = 0; i < incomingFrameBufferSize_/4; ++i)
            {
                // little endian on mac
                // incomingFrameBuffer_[0] [1] [2] [3]
                //                      r   g   b   a
                // if read as int32 -> 0xabgr
                //
                // need to be stored as
                // incomingFrameBuffer_[0] [1] [2] [3]
                //                      a   r   g   b
                // which is as int32 -> 0xbgra
                //
                // in other words, need to convert one int32 (0xabgr)
                // into another int32 (0xbgra)
                
                int idx = i*4;
#if 1
                unsigned char a = incomingFrameBuffer_[idx+3];
                for (int j = 3; j > 0; j--)
                    incomingFrameBuffer_[idx+j] = incomingFrameBuffer_[idx+j-1];
                incomingFrameBuffer_[idx] = a;
#else
                // this code does not work, i can't figure out why
                int32_t rgba = *(int32_t*)&(incomingFrameBuffer_[idx]);
                int32_t argb = (rgba >> 24 | rgba << 8);
                *((int32_t*)&incomingFrameBuffer_[idx]) = argb;
#endif
            }
            publbishedFrame_ = ((ndnrtc::LocalVideoStream*)stream_)->incomingArgbFrame(topInput->width, topInput->height,
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
            }
            idx++;
        }
    }
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
        targetBitrate.defaultValues[0] = 1000;
        targetBitrate.minSliders[0] = 500;
        targetBitrate.maxSliders[0] = 15000;
        
        encodeWidth.label = "Encode Width";
        encodeWidth.page = "Stream Config";
        encodeWidth.defaultValues[0] = 320;
        encodeWidth.minSliders[0] = 320;
        encodeWidth.maxSliders[0] = 4096;
        
        encodeHeight.label = "Encode Height";
        encodeHeight.page = "Stream Config";
        encodeHeight.defaultValues[0] = 180;
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
        segmentSize.defaultValues[0] = 1200;
        segmentSize.defaultValues[1] = 7700;
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
    if (ndnrtcInitialized_)
    {
        if (stream_)
            ndnrtc_destroyLocalStream(stream_);
        
        LocalStreamParams params = readStreamParams(inputs);
        stream_ = (ndnrtc::LocalVideoStream*)ndnrtc_createLocalStream(params, &NdnrtcStreamLoggingCallback);
        
        free((void*)(params.basePrefix));
        free((void*)(params.streamName));
        free((void*)(params.threadName));
    }
}

LocalStreamParams
ndnrtcOut::readStreamParams(OP_Inputs* inputs) const
{
    LocalStreamParams p;

    string basePrefix = readBasePrefix(inputs);
    p.basePrefix = (const char*)malloc(basePrefix.size()+1);
    strcpy((char*)p.basePrefix, basePrefix.c_str());
    p.streamName = (const char*)malloc(strlen(inputs->getParString(PAR_STREAM_NAME))+1);
    strcpy((char*)p.streamName, inputs->getParString(PAR_STREAM_NAME));
    p.threadName = (const char*)malloc(strlen(inputs->getParString(PAR_THREAD_NAME))+1);
    strcpy((char*)p.threadName, inputs->getParString(PAR_THREAD_NAME));
    
    p.signingOn = inputs->getParInt(PAR_SIGNING);
    p.fecOn = inputs->getParInt(PAR_FEC);
    p.typeIsVideo = 1;
    p.ndnSegmentSize = inputs->getParInt(PAR_SEGSIZE);
    p.ndnDataFreshnessPeriodMs = inputs->getParInt(PAR_FRESHNESS);
    p.frameWidth = inputs->getParInt(PAR_ENCODE_WIDTH);
    p.frameHeight = inputs->getParInt(PAR_ENCODE_HEIGHT);
    p.startBitrate = inputs->getParInt(PAR_TARGET_BITRATE);
    p.maxBitrate = p.startBitrate;
    p.gop = inputs->getParInt(PAR_GOPSIZE);
    p.dropFrames = inputs->getParInt(PAR_FRAMEDROP);
    
    return p;
}

void
ndnrtcOut::allocateIncomingFramebuffer(int w, int h)
{
    incomingFrameBufferSize_ = w*h*4*sizeof(unsigned char);
    incomingFrameBuffer_ = (unsigned char*)realloc(incomingFrameBuffer_, incomingFrameBufferSize_);
    memset(incomingFrameBuffer_, 0, incomingFrameBufferSize_);
}
