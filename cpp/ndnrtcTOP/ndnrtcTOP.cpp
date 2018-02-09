// 
// ndnrtcTOP.cpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2016 Regents of the University of California
//

#include "ndnrtcTOP.hpp"

#include <ndnrtc/c-wrapper.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <cmath>

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


ndnrtcTOP::ndnrtcTOP(const OP_NodeInfo* info) : myNodeInfo(info)
{
	myExecuteCount = 0;
	myStep = 0.0;
}

ndnrtcTOP::~ndnrtcTOP()
{

}

void
ndnrtcTOP::getGeneralInfo(TOP_GeneralInfo* ginfo)
{
	// Uncomment this line if you want the TOP to cook every frame even
	// if none of it's inputs/parameters are changing.
	ginfo->cookEveryFrame = true;
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
	myExecuteCount++;


	double speed = inputs->getParDouble("Speed");
	myStep += speed;

	float brightness = (float)inputs->getParDouble("Brightness");

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
	// brightness
	{
		OP_NumericParameter	np;

		np.name = "Brightness";
		np.label = "Brightness";
		np.defaultValues[0] = 1.0;

		np.minSliders[0] =  0.0;
		np.maxSliders[0] =  1.0;

		np.minValues[0] = 0.0;
		np.maxValues[0] = 1.0;

		np.clampMins[0] = true;
		np.clampMaxes[0] = true;
		
		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// speed
	{
		OP_NumericParameter	np;

		np.name = "Speed";
		np.label = "Speed";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -10.0;
		np.maxSliders[0] =  10.0;
		
		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// pulse
	{
		OP_NumericParameter	np;

		np.name = "Reset";
		np.label = "Reset";
		
		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void
ndnrtcTOP::pulsePressed(const char* name)
{
	if (!strcmp(name, "Reset"))
	{
		myStep = 0.0;
	}


}
