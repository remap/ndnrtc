//
//  ndnrtcIn.cpp
//  ndnrtcIn
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcIn.hpp"

#include <OpenGL/gl3.h>
#include <OpenGL/gl.h>

#include <ndnrtc/helpers/face-processor.hpp>
#include <ndnrtc/helpers/key-chain-manager.hpp>
#include <ndnrtc/name-components.hpp>
#include <ndnrtc/statistics.hpp>

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

#define PAR_STREAM_PREFIX       "Streamprefix"
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

int createVideoTexture(unsigned width, unsigned height, void *frameBuffer);
void renderTexture(GLuint texId, unsigned width, unsigned height, void* data);

//******************************************************************************
class RemoteStreamRenderer : public IExternalRenderer
{
public:
    RemoteStreamRenderer():receivedFrame_(0),
        receivedFrameTimestamp_(0), receivedFrameName_(""),
        frameBufferSize_(0), frameWidth_(0), frameHeight_(0),
        frameBuffer_(nullptr), frameUpdated_(false) {}
    
    ~RemoteStreamRenderer() { if (frameBuffer_) free(frameBuffer_); }
    
    const int getWidth() const { return frameWidth_; }
    const int getHeight() const { return frameHeight_; }
    const int getFrameNo() const { return receivedFrame_; }
    const int64_t getTimestamp() const { return receivedFrameTimestamp_; }
    const string& getFrameName() const { return receivedFrameName_; }
    const bool isFrameUpdated() const { return frameUpdated_; }
    const uint8_t* getPixel(int x, int y) const { return &frameBuffer_[4*(y*frameWidth_+x)]; }
    
    // will render current texture if it was updated
    // it is expected that this method is called on the main thread (from execute())
    void renderIfUpdated();
    
    boost::atomic<bool>     bufferRead_;
    
private:
    int                     receivedFrame_;
    int64_t                 receivedFrameTimestamp_;
    string                  receivedFrameName_;
    bool                    frameUpdated_, textureUpToDate_;
    
    boost::atomic<bool>     bufferWrite_;
    int                     frameBufferSize_, frameWidth_, frameHeight_;
    unsigned                texture_;
    GLuint                  depthBuffer_;
    unsigned char*          frameBuffer_;
    
    uint8_t*                getFrameBuffer(int width, int height) override;
    void                    renderBGRAFrame(const FrameInfo& frameInfo, int width, int height,
                                            const uint8_t* buffer) override;
    void                    allocateFramebuffer(int w, int h);
    void                    initTexture();
};

//******************************************************************************
ndnrtcIn::ndnrtcIn(const OP_NodeInfo *info) :
ndnrtcTOPbase(info),
streamRenderer_(boost::make_shared<RemoteStreamRenderer>())
{
    name_ = generateName("ndnrtcIn");
    statStorage_ = StatisticsStorage::createConsumerStatistics();
    init();
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
    format->width = streamRenderer_->getWidth();
    format->height = streamRenderer_->getHeight();
    return true;
}

void
ndnrtcIn::execute(const TOP_OutputFormatSpecs* outputFormat,
                  OP_Inputs* inputs,
                  TOP_Context* context)
{
    ndnrtcTOPbase::execute(outputFormat, inputs, context);
    
    if (streamRenderer_->isFrameUpdated())
    {
//        context->beginGLCommands();

//        glBindFramebuffer(GL_FRAMEBUFFER, context->getFBOIndex());
        int textureMemoryLocation = 0;
        float* mem = (float*)outputFormat->cpuPixelData[textureMemoryLocation];
        streamRenderer_->bufferRead_ = true;
        for (int y = 0; y < outputFormat->height; ++y)
        {
            for (int x = 0; x < outputFormat->width; ++x)
            {
                // ARGB
                const uint8_t framePixel[4] = {255, 0, 0, 255}; // streamRenderer_->getPixel(x, y);
                // RGBA
                float* pixel = &mem[4*(y*outputFormat->width + x)];
                
//                pixel[0] = 0; //(float)(framePixel[1])/255.;
//                pixel[1] = 1; //(float)(framePixel[2])/255.;
//                pixel[2] = 0; //(float)(framePixel[3])/255.;
//                pixel[3] = 1; // (float)(framePixel[0])/255.;
            }
        }
        streamRenderer_->bufferRead_ = false;
        
        outputFormat->newCPUPixelDataLocation = textureMemoryLocation;
        textureMemoryLocation = !textureMemoryLocation;
        
//        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
//        context->endGLCommands();
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
                    chan->value = (float)streamRenderer_->getFrameNo();
                }
                break;
            case InfoChopIndex::ReceivedFrameTimestamp:
                {
                    chan->name = ChanNames[idx].c_str();
                    chan->value = (float)streamRenderer_->getTimestamp();
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
        OP_StringParameter streamPrefix(PAR_STREAM_PREFIX);
        OP_NumericParameter reset("Reset");
        
        streamPrefix.label = "Stream Prefix";
        streamPrefix.defaultValue = "";
        streamPrefix.page = "Stream Config";
        
        reset.label = "Reset";
        reset.page = "Stream Config";
        
        OP_ParAppendResult res = manager->appendString(streamPrefix);
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
ndnrtcIn::init()
{
    ndnrtcTOPbase::init();
    executeQueue_.push(bind(&ndnrtcIn::createRemoteStream, this, _1, _2, _3));
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
    if (!(faceProcessor_ && keyChainManager_))
        return;
 
    errorString_ = "";
    
    const char* streamPrefix = (const char*)malloc(strlen(inputs->getParString(PAR_STREAM_PREFIX))+1);
    strcpy((char*)streamPrefix, inputs->getParString(PAR_STREAM_PREFIX));
    ndn::Name prefix(streamPrefix);
    free((void*)streamPrefix);

    NamespaceInfo prefixInfo;
    
    if (!NameComponents::extractInfo(prefix, prefixInfo) ||
        prefixInfo.streamName_ == "" ||
        prefixInfo.streamType_ == MediaStreamParams::MediaStreamTypeAudio)
    {
        errorString_ = "Invalid stream prefix provided";
        return ;
    }
    
    if (stream_)
    {
        dynamic_pointer_cast<RemoteStream>(stream_)->unregisterObserver(this);
        stream_.reset();
    }
    
    stream_ = boost::make_shared<RemoteVideoStream>(faceProcessor_->getIo(),
                                                    faceProcessor_->getFace(),
                                                    keyChainManager_->instanceKeyChain(),
                                                    prefixInfo.getPrefix(prefix_filter::Base).toUri(),
                                                    prefixInfo.streamName_,
                                                    inputs->getParInt(PAR_LIFETIME),
                                                    inputs->getParInt(PAR_JITTER));
    stream_->setLogger(logger_);
    dynamic_pointer_cast<RemoteStream>(stream_)->registerObserver(this);
    dynamic_pointer_cast<RemoteVideoStream>(stream_)->start("t1", streamRenderer_.get());
}

void
ndnrtcIn::onNewEvent(const ndnrtc::RemoteStream::Event &event)
{
    // handle new event
    LogInfoC << "new event " << event << endl;
}

//******************************************************************************
void
RemoteStreamRenderer::allocateFramebuffer(int w, int h)
{
    frameWidth_ = w; frameHeight_ = h;
    frameBufferSize_ = w*h*4*sizeof(unsigned char);
    frameBuffer_ = (unsigned char*)realloc(frameBuffer_, frameBufferSize_);
    memset(frameBuffer_, 0, frameBufferSize_);
    textureUpToDate_ = false;
}

uint8_t*
RemoteStreamRenderer::getFrameBuffer(int width, int height)
{
    if (bufferRead_)
    {
        cout << "oopsie" << endl;
        return nullptr;
    }

    bufferWrite_ = true;
    
    if (frameWidth_ != width || frameHeight_ != height)
        allocateFramebuffer(width, height);
    return frameBuffer_;
}

void
RemoteStreamRenderer::renderBGRAFrame(const FrameInfo& frameInfo, int width, int height,
                                      const uint8_t* buffer)
{
    frameUpdated_ = true;
    receivedFrameTimestamp_ = frameInfo.timestamp_;
    receivedFrame_ = frameInfo.playbackNo_;
    receivedFrameName_ = frameInfo.ndnName_;
    
    // let know that we have new frame ready for render
    bufferWrite_ = false;
}

void
RemoteStreamRenderer::renderIfUpdated()
{
    if (!bufferWrite_) // && frameUpdated_)
    {
//        if (!textureUpToDate_)
            initTexture();

        bufferRead_ = true;
        renderTexture(texture_, frameWidth_, frameHeight_, frameBuffer_);
        frameUpdated_ = false;
        bufferRead_ = false;
    }
}

void
RemoteStreamRenderer::initTexture()
{
    if (glIsTexture(texture_))
    {
        glDeleteTextures(1, (const GLuint*)&texture_);
        GetError();
        texture_ = 0;
    }
    
    texture_ = createVideoTexture(frameWidth_, frameHeight_, frameBuffer_);
    textureUpToDate_ = true;
    
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           texture_,
                           0);
    
//    glGenRenderbuffers(1, &depthBuffer_);
//    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
//    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1024, 768);
//    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer_);
    
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);
}

//******************************************************************************
int
createVideoTexture(unsigned width, unsigned height, void *frameBuffer)
{
    int texture = 0;
    
    glGenTextures(1, (GLuint*)&texture);
    GetError();
    
    glBindTexture(GL_TEXTURE_2D, texture);
    GetError();
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GetError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GetError();
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    GetError();
    
    return texture;
}

void
renderTexture(GLuint texId, unsigned width, unsigned height, void* data)
{
#if 1
    glViewport(0, 0, width, height);
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
//    cout << "viewport" << endl;
    GetError()

//    glEnable(GL_TEXTURE_2D);
//    cout << "gl enable" << endl;
//    GetError()
    
    glBindTexture(GL_TEXTURE_2D, texId);
    cout << "bind texture" << endl;
    GetError()
    
//    glLoadIdentity();
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    cout << "sub image" << endl;
    GetError()
    
//    glBegin(GL_QUADS);
//    GetError()
//    // the reason why texture coordinates are weird - the texture is flipped horizontally
//    glTexCoord2f(0., 1.); glVertex2i(0, 0);
//    glTexCoord2f(1., 1.); glVertex2i(width, 0);
//    glTexCoord2f(1., 0.); glVertex2i(width, height);
//    glTexCoord2f(0., 0.); glVertex2i(0, height);
//    GetError()
//    glEnd();
//    GetError()
#endif
}
