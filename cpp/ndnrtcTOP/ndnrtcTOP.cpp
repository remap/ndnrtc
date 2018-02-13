// 
// ndnrtcTOP.cpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2016 Regents of the University of California
//

#include "ndnrtcTOP.hpp"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>

#include "foundation-helpers.h"

using namespace std;
using namespace std::placeholders;

#define PAR_NFD_HOST            "Nfdhost"
#define PAR_SIGNING_IDENTITY    "Signingidentity"
#define PAR_INSTANCE_NAME       "Instancename"
#define PAR_USE_MACOS_KEYCHAIN  "Usemacoskeychain"
#define PAR_INIT                "Init"
#define PAR_BASE_PREFIX         "Baseprefix"
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
	return new ndnrtcTOP(info);
}

DLLEXPORT
void
DestroyTOPInstance(TOP_CPlusPlusBase* instance, TOP_Context *context)
{
	// Delete the instance here, this will be called when
	// Touch is shutting down, when the TOP using that instance is deleted, or
	// if the TOP loads a different DLL
	delete (ndnrtcTOP*)instance;
}

};

//******************************************************************************
static void NdnrtcLoggingCallback(const char* message)
{
    std::cout << "[ndnrtc] " << message << endl;
}

//******************************************************************************
ndnrtcTOP::ndnrtcTOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myStep = 0.0;
}

ndnrtcTOP::~ndnrtcTOP()
{
    deinitNdnrtcLibrary();
}

void
ndnrtcTOP::getGeneralInfo(TOP_GeneralInfo* ginfo)
{
	// Uncomment this line if you want the TOP to cook every frame even
	// if none of it's inputs/parameters are changing.
	ginfo->cookEveryFrame = false;
    ginfo->memPixelType = OP_CPUMemPixelType::RGBA32Float;
}

bool
ndnrtcTOP::getOutputFormat(TOP_OutputFormat* format)
{
	// In this function we could assign variable values to 'format' to specify
	// the pixel format/resolution etc that we want to output to.
	// If we did that, we'd want to return true to tell the TOP to use the settings we've
	// specified.
	// In this example we'll return false and use the TOP's settings
	return false;
}

void
ndnrtcTOP::execute(const TOP_OutputFormatSpecs* outputFormat,
						OP_Inputs* inputs,
						TOP_Context *context)
{
    checkInputs(outputFormat, inputs, context);
    
    while (executeQueue_.size())
    {
        executeQueue_.front()(outputFormat, inputs, context);
        executeQueue_.pop();
    }
    
	myExecuteCount++;

    double speed = 1; //inputs->getParDouble("Speed");
	myStep += speed;

    float brightness = 1; //(float)inputs->getParDouble("Brightness");

	int xstep = (int)(fmod(myStep, outputFormat->width));
	int ystep = (int)(fmod(myStep, outputFormat->height));

	if (xstep < 0)
		xstep += outputFormat->width;

	if (ystep < 0)
		ystep += outputFormat->height;


    int textureMemoryLocation = 0;

    float* mem = (float*)outputFormat->cpuPixelData[textureMemoryLocation];

    for (int y = 0; y < outputFormat->height; ++y)
    {
        for (int x = 0; x < outputFormat->width; ++x)
        {
            float* pixel = &mem[4*(y*outputFormat->width + x)];

			// RGBA
            pixel[0] = (x > xstep) * brightness;
            pixel[1] = (y > ystep) * brightness;
            pixel[2] = ((float) (xstep % 50) / 50.0f) * brightness;
            pixel[3] = 1;
        }
    }

    outputFormat->newCPUPixelDataLocation = textureMemoryLocation;
    textureMemoryLocation = !textureMemoryLocation;
}

int32_t
ndnrtcTOP::getNumInfoCHOPChans()
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the TOP. In this example we are just going to send one channel.
	return 2;
}

void
ndnrtcTOP::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name = "executeCount";
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name = "step";
		chan->value = (float)myStep;
	}
}

bool		
ndnrtcTOP::getInfoDATSize(OP_InfoDATSize* infoSize)
{
	infoSize->rows = 2;
	infoSize->cols = 2;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
ndnrtcTOP::getInfoDATEntries(int32_t index,
								int32_t nEntries,
								OP_InfoDATEntries* entries)
{
	// It's safe to use static buffers here because Touch will make it's own
	// copies of the strings immediately after this call returns
	// (so the buffers can be reuse for each column/row)
	static char tempBuffer1[4096];
	static char tempBuffer2[4096];

	if (index == 0)
	{
        // Set the value for the first column
#ifdef WIN32
        strcpy_s(tempBuffer1, "executeCount");
#else // macOS
        strlcpy(tempBuffer1, "executeCount", sizeof(tempBuffer1));
#endif
        entries->values[0] = tempBuffer1;

        // Set the value for the second column
#ifdef WIN32
        sprintf_s(tempBuffer2, "%d", myExecuteCount);
#else // macOS
        snprintf(tempBuffer2, sizeof(tempBuffer2), "%d", myExecuteCount);
#endif
        entries->values[1] = tempBuffer2;
	}

	if (index == 1)
	{
#ifdef WIN32
		strcpy_s(tempBuffer1, "step");
#else // macOS
        strlcpy(tempBuffer1, "ndnrtc lib version", sizeof(tempBuffer1));
#endif
		entries->values[0] = tempBuffer1;

#ifdef WIN32
		sprintf_s(tempBuffer2, "%g", myStep);
#else // macOS
        const char *ndnrtcLibVersion = ndnrtc_lib_version();
        snprintf(tempBuffer2, strlen(ndnrtcLibVersion), "%s", ndnrtcLibVersion);
#endif
		entries->values[1] = tempBuffer2;
	}
}

void
ndnrtcTOP::setupParameters(OP_ParameterManager* manager)
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
        instanceName.defaultValue = "ndnrtcTOP0";
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
    {
        OP_StringParameter basePrefix(PAR_BASE_PREFIX), streamName(PAR_STREAM_NAME);
        OP_NumericParameter targetBitrate(PAR_TARGET_BITRATE), encodeWidth(PAR_ENCODE_WIDTH),
                            encodeHeight(PAR_ENCODE_HEIGHT);
        
        basePrefix.label = "Base prefix";
        basePrefix.defaultValue = "/touch/ndnrtc";
        basePrefix.page = "Stream Config";
        
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
        
        OP_ParAppendResult res = manager->appendString(basePrefix);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendString(streamName);
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
ndnrtcTOP::pulsePressed(const char* name)
{
	if (!strcmp(name, "Init"))
	{
        executeQueue_.push(bind(&ndnrtcTOP::initNdnrtcLibrary, this, _1, _2, _3));
	}
}

//******************************************************************************
#pragma mark private
void
ndnrtcTOP::checkInputs(const TOP_OutputFormatSpecs* outputFormat,
                       OP_Inputs* inputs,
                       TOP_Context *context)
{
    bool useMacOsKeyChain = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    inputs->enablePar(PAR_SIGNING_IDENTITY, !useMacOsKeyChain);
}

void
ndnrtcTOP::initNdnrtcLibrary(const TOP_OutputFormatSpecs* outputFormat,
                             OP_Inputs* inputs,
                             TOP_Context *context)
{
    if (ndnrtcInitialized_)
    {
        ndnrtc_deinit();
    }
    
    std::string hostname(inputs->getParString(PAR_NFD_HOST));
    std::string signingIdentity(inputs->getParString(PAR_SIGNING_IDENTITY));
    std::string instanceName(inputs->getParString(PAR_INSTANCE_NAME));
    bool useMacOsKeyChain = inputs->getParInt(PAR_USE_MACOS_KEYCHAIN);
    
    try
    {
        if (useMacOsKeyChain)
            ndnrtcInitialized_ = ndnrtc_init(hostname.c_str(), nullptr,
                                             signingIdentity.c_str(), instanceName.c_str(),
                                             &NdnrtcLoggingCallback);
        else if (get_resources_path())
        {
            inputs->enablePar(PAR_SIGNING_IDENTITY, false);
            
            string keyChainPath = string(get_resources_path())+"/keychain";
            ndnrtcInitialized_ = ndnrtc_init(hostname.c_str(), keyChainPath.c_str(),
                                             TOUCHDESIGNER_IDENTITY, instanceName.c_str(),
                                             &NdnrtcLoggingCallback);
        }
        else
        {
            // TODO report error!
            cout << "error: file-bbased keychain was not found" << endl;
        }
    }
    catch(std::exception &e)
    {
        cout << "Error initializing library: " << e.what() << endl;
    }
}

void
ndnrtcTOP::deinitNdnrtcLibrary()
{
    ndnrtc_deinit();
}

LocalStreamParams
ndnrtcTOP::readStreamParams() const
{
    LocalStreamParams p;
    
    return p;
}
