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

#define PAR_FRAME_RESOLUTION    "Frameresolution"
#define PAR_TARGET_BITRATE      "Targetbitrate"

#if USE_ENCODE_RESOLUTION
#define PAR_ENCODE_WIDTH        "Encodewidth"
#define PAR_ENCODE_HEIGHT       "Encodeheight"
#endif

#define PAR_FEC                 "Fec"
#define PAR_SIGNING             "Signing"
#define PAR_FRAMEDROP           "Framedrop"
#define PAR_SEGSIZE             "Segmentsize"
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

void
FillTOPPluginInfo(TOP_PluginInfo *info)
{
    // This must always be set to this constant
    info->apiVersion = TOPCPlusPlusAPIVersion;
    
    // Change this to change the executeMode behavior of this plugin.
    info->executeMode = TOP_ExecuteMode::CPUMemWriteOnly;
    
    // The opType is the unique name for this TOP. It must start with a
    // capital A-Z character, and all the following characters must lower case
    // or numbers (a-z, 0-9)
    info->customOPInfo.opType->setString("Ndnrtcout");
    
    // The opLabel is the text that will show up in the OP Create Dialog
    info->customOPInfo.opLabel->setString("NDN-RTC Out");
    
    // Will be turned into a 3 letter icon on the nodes
    info->customOPInfo.opIcon->setString("NRT");
    
    // Information about the author of this OP
    info->customOPInfo.authorName->setString("Peter Gusev");
    info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
    
    // This TOP works with 0 or 1 inputs connected
    info->customOPInfo.minInputs = 0;
    info->customOPInfo.maxInputs = 1;
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
/**
 * This enum identifies output DAT's different fields
 * The output DAT is a table that contains two
 * columns : name(identified by this enum) and value
 * (either float or string)
 */
enum class InfoChopIndex {
    PublishedFrame,
    PrefixRegistered
};


/**
 * This maps output CHOP's channel names
 */
static std::map<InfoChopIndex, std::string> ChanNames = {
    { InfoChopIndex::PublishedFrame, "publishedFrame" },
    { InfoChopIndex::PrefixRegistered, "prefixRegistered" }
};

/**
 * This maps output DAT's fields onto their textual representation
 */
enum class InfoDatIndex {
    PublishedFrameName,
    PublishedFrameTimestamp
};

static std::map<InfoDatIndex, std::string> RowNames = {
    { InfoDatIndex::PublishedFrameName, "Published Frame Name" },
    { InfoDatIndex::PublishedFrameTimestamp, "Published Frame Timestamp" }
};

/**
 * These are the parameters that will affect whether TOP must be re-initialized or not
 */
#if USE_ENCODE_RESOLUTION
static set<string> ReinitParams({PAR_FRAME_RESOLUTION, PAR_FEC, PAR_GOPSIZE, PAR_SEGSIZE, PAR_SIGNING, PAR_FRAMEDROP, PAR_TARGET_BITRATE, PAR_ENCODE_WIDTH, PAR_ENCODE_HEIGHT});
#else
static set<string> ReinitParams({PAR_FRAME_RESOLUTION, PAR_FEC, PAR_GOPSIZE, PAR_SEGSIZE, PAR_SIGNING, PAR_FRAMEDROP, PAR_TARGET_BITRATE});
#endif

//******************************************************************************
ndnrtcOut::ndnrtcOut(const OP_NodeInfo* info) :
ndnrtcTOPbase(info),
incomingFrameBuffer_(nullptr),
incomingFrameWidth_(0), incomingFrameHeight_(0)
{
    params_ = new ndnrtcOut::Params();
    memset((void*)params_, 0, sizeof(Params));
    reinitParams_.insert(ReinitParams.begin(), ReinitParams.end());
    statStorage_ = StatisticsStorage::createProducerStatistics();
}

ndnrtcOut::~ndnrtcOut()
{
    free(params_);
}

void
ndnrtcOut::getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs*, void *reserved1)
{
	// Uncomment this line if you want the TOP to cook every frame even
	// if none of it's inputs/parameters are changing.
	ginfo->cookEveryFrame = true;
    ginfo->memPixelType = OP_CPUMemPixelType::BGRA8Fixed;
}

bool
ndnrtcOut::getOutputFormat(TOP_OutputFormat* format, const OP_Inputs*, void *reserved1)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings
	return false;
}

void
ndnrtcOut::execute(TOP_OutputFormatSpecs* outputFormat,
                   const OP_Inputs* inputs,
                   TOP_Context *context,
                   void *reserved1)
{
    ndnrtcTOPbase::execute(outputFormat, inputs, context, reserved1);
    
    int nInputs = inputs->getNumInputs();
    
    if (nInputs)
    {
        const OP_TOPInput* topInput = inputs->getInputTOP(0);
        
        if (topInput && stream_)
        {
            glBindTexture(GL_TEXTURE_2D, topInput->textureIndex);
            GetError()
            glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, incomingFrameBuffer_);
            GetError();
            
            // TODO: to update preview, need to get frame sizes from the input and then set them for outputFormat
            // on the next run (need to save input sizes, like "prevInputWidth = topInput->width;")
//            memcpy(outputFormat->cpuPixelData[0], incomingFrameBuffer_, incomingFrameBufferSize_);
            int fNo = boost::dynamic_pointer_cast<LocalVideoStream>(stream_)->incomingBgraFrame(topInput->width, topInput->height,
                                                                                      incomingFrameBuffer_, incomingFrameBufferSize_);
            if (fNo >= 0)
            {
                publishedFrame_ = fNo;
                map<string, FrameInfo> m = boost::dynamic_pointer_cast<LocalVideoStream>(stream_)->getLastPublishedInfo();
                publishedFrameInfo_ = m.begin()->second;
            }
            
        }
    }
}

int32_t
ndnrtcOut::getNumInfoCHOPChans(void *reserved1)
{
	return (int32_t)ChanNames.size() + (int32_t)statStorage_->getIndicators().size();
}

void
ndnrtcOut::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void *reserved1)
{
    InfoChopIndex idx = (InfoChopIndex)index;
    
    if (ChanNames.find(idx) != ChanNames.end())
    {
        switch (idx) {
            case InfoChopIndex::PublishedFrame:
            {
                chan->name->setString(ChanNames[idx].c_str());
                chan->value = (float)publishedFrame_;
            }
                break;
            case InfoChopIndex::PrefixRegistered:
            {
                chan->name->setString(ChanNames[idx].c_str());
                chan->value = (float)prefixRegistered_;
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
            chan->name->setString(StatisticsStorage::IndicatorKeywords.at(pair.first).c_str());
                chan->value = (float)pair.second;
                break;
            }
            idx++;
        } // for
    } // else
}

bool
ndnrtcOut::getInfoDATSize(OP_InfoDATSize* infoSize, void *reserved1)
{
    ndnrtcTOPbase::getInfoDATSize(infoSize, reserved1);
    infoSize->rows += RowNames.size();
    return true;
}

void
ndnrtcOut::getInfoDATEntries(int32_t index,
                             int32_t nEntries,
                             OP_InfoDATEntries* entries,
                             void *reserved1)
{
    if (index >= RowNames.size())
        ndnrtcTOPbase::getInfoDATEntries(index-(int32_t)RowNames.size(), nEntries, entries, reserved1);
    else
    {
        InfoDatIndex idx = (InfoDatIndex)index;
        
        if (RowNames.find(idx) != RowNames.end())
        {
            entries->values[0]->setString(RowNames[idx].c_str());
            switch (idx)
            {
                case InfoDatIndex::PublishedFrameName:
                {
                    entries->values[1]->setString(publishedFrameInfo_.ndnName_.c_str());
                }
                    break;
                case InfoDatIndex::PublishedFrameTimestamp:
                {
                    static char tempBuffer[4096];
                    memset(tempBuffer, 0, 4096);
                    sprintf(tempBuffer, "%f", ((double)publishedFrameInfo_.timestamp_/1000.));
                    entries->values[1]->setString(tempBuffer);
                }
                    break;
                default: break;
            }
        }
    }
}

void
ndnrtcOut::setupParameters(OP_ParameterManager* manager, void *reserved1)
{
    ndnrtcTOPbase::setupParameters(manager, reserved1);
    
    {
        OP_NumericParameter targetBitrate(PAR_TARGET_BITRATE);
#if USE_ENCODE_RESOLUTION
        OP_NumericParameter encodeWidth(PAR_ENCODE_WIDTH),
                            encodeHeight(PAR_ENCODE_HEIGHT);
#endif
        targetBitrate.label = "Target Bitrate";
        targetBitrate.page = "Stream Config";
        targetBitrate.defaultValues[0] = 3000;
        targetBitrate.minSliders[0] = 500;
        targetBitrate.maxSliders[0] = 15000;
        
#if USE_ENCODE_RESOLUTION
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
#endif
        
        OP_ParAppendResult res = manager->appendInt(targetBitrate);
        assert(res == OP_ParAppendResult::Success);
#if USE_ENCODE_RESOLUTION
        res = manager->appendInt(encodeWidth);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(encodeHeight);
        assert(res == OP_ParAppendResult::Success);
#endif
    }
    {
        OP_NumericParameter fec(PAR_FEC), signing(PAR_SIGNING), frameDrop(PAR_FRAMEDROP),
                            segmentSize(PAR_SEGSIZE),
                            gopSize(PAR_GOPSIZE);
        
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

        OP_ParAppendResult res = manager->appendToggle(fec);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(signing);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(frameDrop);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(segmentSize);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendInt(gopSize);
        assert(res == OP_ParAppendResult::Success);
    }
}

void
ndnrtcOut::pulsePressed(const char* name, void *reserved1)
{
    ndnrtcTOPbase::pulsePressed(name, reserved1);
    
	if (!strcmp(name, "Init"))
	{
        // no need to re-create the stream, this is called in the base class
//        executeQueue_.push(bind(&ndnrtcOut::createLocalStream, this, _1, _2, _3));
	}
}

//******************************************************************************
#pragma mark private
void
ndnrtcOut::init()
{
    ndnrtcTOPbase::init();
    
    executeQueue_.push(bind(&ndnrtcOut::registerPrefix, this, _1, _2, _3));
    executeQueue_.push([this](TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs, TOP_Context *context)
    {
        if (keyChainManager_)
            keyChainManager_->publishCertificates();
    });
    
}

void
ndnrtcOut::initStream()
{
    executeQueue_.push(bind(&ndnrtcOut::createLocalStream, this, _1, _2, _3));
}

std::set<std::string>
ndnrtcOut::checkInputs(TOP_OutputFormatSpecs* outputFormat,
                       const OP_Inputs* inputs,
                       TOP_Context *context)
{
    set<string> updatedParams = ndnrtcTOPbase::checkInputs(outputFormat, inputs, context);
    
    if (inputs->getNumInputs())
    {
        const OP_TOPInput *topInput = inputs->getInputTOP(0);
        if (topInput)
            if (topInput->width != incomingFrameWidth_ || topInput->height != incomingFrameHeight_)
            {
                allocateIncomingFramebuffer(topInput->width, topInput->height);
                updatedParams.insert(PAR_FRAME_RESOLUTION);
            }
    }
    
    if (((ndnrtcOut::Params*)params_)->targetBitrate_ != inputs->getParInt(PAR_TARGET_BITRATE))
    {
        updatedParams.insert(PAR_TARGET_BITRATE);
        ((ndnrtcOut::Params*)params_)->targetBitrate_ = inputs->getParInt(PAR_TARGET_BITRATE);
    }
    
    if (((ndnrtcOut::Params*)params_)->fecEnabled_!= inputs->getParInt(PAR_FEC))
    {
        updatedParams.insert(PAR_FEC);
        ((ndnrtcOut::Params*)params_)->fecEnabled_ = inputs->getParInt(PAR_FEC);
    }
    
    if (((ndnrtcOut::Params*)params_)->signingEnabled_ != inputs->getParInt(PAR_SIGNING))
    {
        updatedParams.insert(PAR_SIGNING);
        ((ndnrtcOut::Params*)params_)->signingEnabled_ = inputs->getParInt(PAR_SIGNING);
    }
    
    if (((ndnrtcOut::Params*)params_)->frameDropAllowed_ != inputs->getParInt(PAR_FRAMEDROP))
    {
        updatedParams.insert(PAR_FRAMEDROP);
        ((ndnrtcOut::Params*)params_)->frameDropAllowed_ = inputs->getParInt(PAR_FRAMEDROP);
    }
    
    if (((ndnrtcOut::Params*)params_)->segmentSize_ != inputs->getParInt(PAR_SEGSIZE))
    {
        updatedParams.insert(PAR_SEGSIZE);
        ((ndnrtcOut::Params*)params_)->segmentSize_ = inputs->getParInt(PAR_SEGSIZE);
    }
    
    if (((ndnrtcOut::Params*)params_)->gopSize_ != inputs->getParInt(PAR_GOPSIZE))
    {
        updatedParams.insert(PAR_GOPSIZE);
        ((ndnrtcOut::Params*)params_)->gopSize_ = inputs->getParInt(PAR_GOPSIZE);
    }
    
#if USE_ENCODE_RESOLUTION
    if (((ndnrtcOut::Params*)params_)->encodeWidth_ != inputs->getParInt(PAR_ENCODE_WIDTH))
    {
        updatedParams.insert(PAR_ENCODE_WIDTH);
        ((ndnrtcOut::Params*)params_)->encodeWidth_ = inputs->getParInt(PAR_ENCODE_WIDTH);
    }
    
    if (((ndnrtcOut::Params*)params_)->encodeHeight_ != inputs->getParInt(PAR_ENCODE_HEIGHT))
    {
        updatedParams.insert(PAR_ENCODE_HEIGHT);
        ((ndnrtcOut::Params*)params_)->encodeHeight_ = inputs->getParInt(PAR_ENCODE_HEIGHT);
    }
#endif
    
    return updatedParams;
}

void
ndnrtcOut::createLocalStream(TOP_OutputFormatSpecs *outputFormat,
                             const OP_Inputs *inputs,
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
    
    LogInfoC << "creating local stream with parameters: " << streamSettings.params_ << endl;
    stream_ = boost::make_shared<LocalVideoStream>(readBasePrefix(inputs),
                                                   streamSettings,
                                                   inputs->getParInt(PAR_FEC));
    stream_->setLogger(streamLogger_);
}

MediaStreamParams
ndnrtcOut::readStreamParams(const OP_Inputs* inputs) const
{
    MediaStreamParams p;
    p.type_ = MediaStreamParams::MediaStreamTypeVideo;
    p.producerParams_.segmentSize_ = inputs->getParInt(PAR_SEGSIZE);
    p.producerParams_.freshness_ = {15, 30, 900};
    p.streamName_ = ndnrtcTOPbase::NdnRtcStreamName;
    
    VideoCoderParams vcp;
    vcp.codecFrameRate_ = 30; // TODO: try 60fps;
    vcp.gop_ = inputs->getParInt(PAR_GOPSIZE);
    vcp.startBitrate_ = inputs->getParInt(PAR_TARGET_BITRATE);
    vcp.maxBitrate_ = inputs->getParInt(PAR_TARGET_BITRATE);
#if USE_ENCODE_RESOLUTION
    vcp.encodeWidth_ = inputs->getParInt(PAR_ENCODE_WIDTH);
    vcp.encodeHeight_ = inputs->getParInt(PAR_ENCODE_HEIGHT);
#else
    vcp.encodeWidth_ = incomingFrameWidth_;
    vcp.encodeHeight_ = incomingFrameHeight_;
#endif
    vcp.dropFramesOn_ =  inputs->getParInt(PAR_FRAMEDROP);
    
    p.addMediaThread(VideoThreadParams(ndnrtcTOPbase::NdnRtcTrheadName, vcp));
    
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
