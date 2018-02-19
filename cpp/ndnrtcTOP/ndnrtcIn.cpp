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
ndnrtcIn::ndnrtcIn(const OP_NodeInfo *info):ndnrtcTOPbase(info)
{
    
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
//    format->width
//    format->height
    return false;
}

void
ndnrtcIn::execute(const TOP_OutputFormatSpecs* outputFormat,
                            OP_Inputs* inputs,
                            TOP_Context* context)
{
    ndnrtcTOPbase::execute(outputFormat, inputs, context);
}

int32_t
ndnrtcIn::getNumInfoCHOPChans()
{
    return 0;
}

void
ndnrtcIn::getInfoCHOPChan(int32_t index,
                          OP_InfoCHOPChan *chan)
{
    
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
}

void
ndnrtcIn::pulsePressed(const char *name)
{
    ndnrtcTOPbase::pulsePressed(name);
}

void
ndnrtcIn::checkInputs(const TOP_OutputFormatSpecs* outputFormat,
                      OP_Inputs* inputs,
                      TOP_Context *context)
{
    ndnrtcTOPbase::checkInputs(outputFormat, inputs, context);
}

void
ndnrtcIn::createRemoteStream(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *)
{
    
}
