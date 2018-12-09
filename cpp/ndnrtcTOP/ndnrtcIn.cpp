//
//  ndnrtcIn.cpp
//  ndnrtcIn
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcIn.hpp"

#include <mutex>

#include <OpenGL/gl3.h>
#include <OpenGL/gl.h>

#include <ndnrtc/helpers/face-processor.hpp>
#include <ndnrtc/helpers/key-chain-manager.hpp>
#include <ndnrtc/name-components.hpp>
#include <ndnrtc/statistics.hpp>
#include <ndnrtc/frame-fetcher.hpp>

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

#define PAR_STREAM_PREFIX1      "Streamprefix1"
#define PAR_STREAM_PREFIX2      "Streamprefix2"
#define PAR_LIFETIME            "Lifetime"
#define PAR_JITTER              "Jittersize"
#define PAR_FETCH_FRAME_TOGGLE  "Fetchframetoggle"

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
    ReceivedFrameIsKey,
    State
};


/**
 * This maps output CHOP's channel names
 */
static std::map<InfoChopIndex, std::string> ChanNames = {
    { InfoChopIndex::ReceivedFrame, "receivedFrame" },
    { InfoChopIndex::ReceivedFrameIsKey, "receivedFrameIsKey" },
    { InfoChopIndex::State, "state" }
};

/**
 * This maps output DAT's fields onto their textual representation
 */
enum class InfoDatIndex {
    ReceivedFrameName,
    ReceivedFrameTimestamp
};

static std::map<InfoDatIndex, std::string> RowNames = {
    { InfoDatIndex::ReceivedFrameName, "Received Frame Name" },
    { InfoDatIndex::ReceivedFrameTimestamp, "Received Frame Timestamp" }
};

int createVideoTexture(unsigned width, unsigned height, void *frameBuffer);
void renderTexture(GLuint texId, unsigned width, unsigned height, void* data);
bool prefixHasFrameLevelInfo(const NamespaceInfo&);

//******************************************************************************
class RemoteStreamRenderer : public IExternalRenderer
{
public:
    class FrameBuffer {
    public:
        FrameBuffer(int w = 1, int h = 1): frameBufferSize_(0),
            frameWidth_(0), frameHeight_(0), frameBuffer_(nullptr) { resize(w, h); }

        FrameBuffer(FrameBuffer&& fb): frameBufferSize_(fb.frameBufferSize_),
            frameWidth_(fb.frameWidth_), frameHeight_(fb.frameHeight_), frameBuffer_(fb.frameBuffer_)
        {
            fb.frameBuffer_ = nullptr;
            fb.frameWidth_ = 0;
            fb.frameHeight_ = 0;
            fb.frameBufferSize_ = 0;
            fb.info_ = FrameInfo();
        }
        
        ~FrameBuffer() { if (frameBuffer_) free(frameBuffer_); }
        
        const int getWidth() const { return frameWidth_; }
        const int getHeight() const { return frameHeight_; }
        const int getSize() const { return frameBufferSize_; }
        const uint8_t* getPixel(int x, int y) const { return &frameBuffer_[4*(y*frameWidth_+x)]; }
        const FrameInfo& getInfo() const { return info_; }
        
        uint8_t* getFrameBuffer() { return frameBuffer_; }
        uint8_t* getFrameBuffer(int w, int h)
        {
            if (w != frameWidth_ || h != frameHeight_) resize(w, h);
            return frameBuffer_;
        }
        
        void resize(int w, int h)
        {
            frameWidth_ = w; frameHeight_ = h;
            int newSize = w*h*4*sizeof(unsigned char);
            if (newSize > frameBufferSize_) // only resize if the new size is larger
                frameBuffer_ = (unsigned char*)realloc(frameBuffer_, newSize);

            frameBufferSize_ = newSize;
            memset(frameBuffer_, 0, frameBufferSize_);
        }
        
        void setInfo(const FrameInfo& info) { info_ = info; }
        
    private:
        FrameBuffer(const FrameBuffer&) = delete;

        int frameBufferSize_, frameWidth_, frameHeight_;
        unsigned char* frameBuffer_;
        FrameInfo info_;
    };
    
    RemoteStreamRenderer(): buffers_(2),
        frontBuffer_(&buffers_[0]), backBuffer_(&buffers_[1]) {}
    
    const int getWidth() const { return frontBuffer_.load()->getWidth(); }
    const int getHeight() const { return frontBuffer_.load()->getHeight(); }
    const int getFrameNo() const { return frontBuffer_.load()->getInfo().playbackNo_; }
    const int64_t getTimestamp() const { return frontBuffer_.load()->getInfo().timestamp_; }
    const string& getFrameName() const { return frontBuffer_.load()->getInfo().ndnName_; }
    const uint8_t* getPixel(int x, int y) const { return frontBuffer_.load()->getPixel(x, y); }
    
    const void readBuffer(function<void(const uint8_t* buffer, const FrameInfo& info)> onBufferAccess);
    
    uint8_t* getFrameBuffer(int width, int height, IExternalRenderer::BufferType*);
    void renderFrame(const FrameInfo& frameInfo, int width, int height, const uint8_t* buffer);
    
private:
    std::mutex                bufferReadMutex_;
    vector<FrameBuffer>       buffers_;
    std::atomic<FrameBuffer*> frontBuffer_, backBuffer_; // don't move these before "buffers_", please
};

/**
 * These are the parameters that will affect whether TOP must be re-initialized or not
 */
static set<string> ReinitParams({PAR_LIFETIME, PAR_JITTER});


//******************************************************************************
ndnrtcIn::ndnrtcIn(const OP_NodeInfo *info) :
ndnrtcTOPbase(info),
streamRenderer_(boost::make_shared<RemoteStreamRenderer>())
{
    params_ = new ndnrtcIn::Params();
    memset((void*)params_, 0, sizeof(Params));
    reinitParams_.insert(ReinitParams.begin(), ReinitParams.end());

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

    if (stream_ || frameFetcher_) // also, maybe, if state is Fetching?
    {
        int textureMemoryLocation = 0;
        uint8_t* mem = (uint8_t*)outputFormat->cpuPixelData[textureMemoryLocation];
        
        streamRenderer_->readBuffer([this, outputFormat, mem](const uint8_t* activeBuffer, const FrameInfo& finfo)
                                    {
                                        receivedFrameInfo_ = finfo;
                                        memcpy(mem, activeBuffer, 4*outputFormat->width*outputFormat->height);
                                    });
        
        outputFormat->newCPUPixelDataLocation = textureMemoryLocation;
        textureMemoryLocation = !textureMemoryLocation;
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
                    chan->value = (float)receivedFrameInfo_.playbackNo_;
                }
                break;
            case InfoChopIndex::State:
                {
                    chan->name = ChanNames[idx].c_str();
                    chan->value = 0;
                }
                break;
            case InfoChopIndex::ReceivedFrameIsKey:
            {
                chan->name = ChanNames[idx].c_str();
                chan->value = receivedFrameInfo_.isKey_;
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
    ndnrtcTOPbase::getInfoDATSize(infoSize);
    infoSize->rows += RowNames.size();
    return true;
}

void
ndnrtcIn::getInfoDATEntries(int32_t index,
                            int32_t nEntries,
                            OP_InfoDATEntries *entries)
{
    if (index >= RowNames.size())
        ndnrtcTOPbase::getInfoDATEntries(index-(int32_t)RowNames.size(), nEntries, entries);
    else
    {
        static char tempBuffer1[4096];
        static char tempBuffer2[4096];
        memset(tempBuffer1, 0, 4096);
        memset(tempBuffer2, 0, 4096);
        
        InfoDatIndex idx = (InfoDatIndex)index;
        
        if (RowNames.find(idx) != RowNames.end())
        {
            strcpy(tempBuffer1, RowNames[idx].c_str());
            
            switch (idx) {
                case InfoDatIndex::ReceivedFrameName:
                {
                    if (stream_)
                        strcpy(tempBuffer2, receivedFrameInfo_.ndnName_.c_str());
                }
                    break;
                case InfoDatIndex::ReceivedFrameTimestamp:
                {
                    sprintf(tempBuffer2, "%f", ((double)receivedFrameInfo_.timestamp_/1000.));
                }
                    break;
                default:
                    break;
            }
            
            entries->values[0] = tempBuffer1;
            entries->values[1] = tempBuffer2;
        }
    }
}

void
ndnrtcIn::setupParameters(OP_ParameterManager *manager)
{
    ndnrtcTOPbase::setupParameters(manager);
    
    {
        OP_StringParameter streamPrefix1(PAR_STREAM_PREFIX1), streamPrefix2(PAR_STREAM_PREFIX2);
        OP_NumericParameter reset("Fetch"), fetchFrameToggle(PAR_FETCH_FRAME_TOGGLE);
        
        streamPrefix1.label = "Prefix 1";
        streamPrefix1.defaultValue = "";
        streamPrefix1.page = "Fetch Config";
        
        streamPrefix2.label = "Prefix 2";
        streamPrefix2.defaultValue = "";
        streamPrefix2.page = "Fetch Config";
        
        reset.label = "Fetch";
        reset.page = "Fetch Config";
        
        fetchFrameToggle.label = "Fetch Frame";
        fetchFrameToggle.page = "Fetch Config";
        
        OP_ParAppendResult res = manager->appendString(streamPrefix1);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendString(streamPrefix2);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendPulse(reset);
        assert(res == OP_ParAppendResult::Success);
        res = manager->appendToggle(fetchFrameToggle);
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
    
    if (!strcmp(name, "Fetch"))
        executeQueue_.push(bind(&ndnrtcIn::startFetch, this, _1, _2, _3));
}

void
ndnrtcIn::initStream()
{
    executeQueue_.push(bind(&ndnrtcIn::startFetch, this, _1, _2, _3));
}

set<string>
ndnrtcIn::checkInputs(const TOP_OutputFormatSpecs* outputFormat,
                      OP_Inputs* inputs,
                      TOP_Context *context)
{
    set<string> updatedParams = ndnrtcTOPbase::checkInputs(outputFormat, inputs, context);
    
    if (((ndnrtcIn::Params*)params_)->startPrefix_ != string(inputs->getParString(PAR_STREAM_PREFIX1)))
    {
        updatedParams.insert(PAR_STREAM_PREFIX1);
        ((ndnrtcIn::Params*)params_)->startPrefix_ = string(inputs->getParString(PAR_STREAM_PREFIX1));
    }

    if (((ndnrtcIn::Params*)params_)->endPrefix_ != string(inputs->getParString(PAR_STREAM_PREFIX2)))
    {
        updatedParams.insert(PAR_STREAM_PREFIX2);
        ((ndnrtcIn::Params*)params_)->endPrefix_ = string(inputs->getParString(PAR_STREAM_PREFIX2));
    }
    
    if (((ndnrtcIn::Params*)params_)->lifetime_ != inputs->getParInt(PAR_LIFETIME))
    {
        updatedParams.insert(PAR_LIFETIME);
        ((ndnrtcIn::Params*)params_)->lifetime_ = inputs->getParInt(PAR_LIFETIME);
    }
    
    if (((ndnrtcIn::Params*)params_)->jitterSize_ != inputs->getParInt(PAR_JITTER))
    {
        updatedParams.insert(PAR_JITTER);
        ((ndnrtcIn::Params*)params_)->jitterSize_ = inputs->getParInt(PAR_JITTER);
    }
    
    return updatedParams;
}

void
ndnrtcIn::startFetch(const TOP_OutputFormatSpecs* outputFormat,
                     OP_Inputs* inputs,
                     TOP_Context *context)
{
    if (!(faceProcessor_ && keyChainManager_))
        return;
    
    if (stream_)
    {
        // TODO: have EXC_BAD_ACCESS here
        //        dynamic_pointer_cast<RemoteStream>(stream_)->unregisterObserver(this);
        stream_.reset();
    }
    
    if (frameFetcher_)
        frameFetcher_.reset();
    
    errorString_ = "";
    
    // by default, we treat input prefix as a base prefix for touchdesigner ndnrtc stream
    // that means, that the full stream name will look like
    //      <PAR_STREAM_PREFIX>/ndnrtc/<API-version>/video/<ndnrtcTOPbase::NdnRtcStreamName>
    // however, if valid ndnrtc prefix can be decoded from the supplied string, i.e. valid
    // stream prefix or frame prefix, then it is used directly for fetching stream or frame
    // respectively
    ndn::Name prefix(string(inputs->getParString(PAR_STREAM_PREFIX1)));
    ndn::Name endPrefix(string(inputs->getParString(PAR_STREAM_PREFIX2)));
    bool fetchFrameOn = (inputs->getParInt(PAR_FETCH_FRAME_TOGGLE) > 0);

    string basePrefix = string(inputs->getParString(PAR_STREAM_PREFIX1));
    string streamName = ndnrtcTOPbase::NdnRtcStreamName;
    NamespaceInfo prefixInfo, endPrefixInfo;
    
    // three options:
    //  - fetch live stream
    //  - fetch historical stream
    //  - fetch single frame
    // this is how these options are differentiated:
    //  - if prefix1 and prefix2 are provided and are frame-level prefixes and key frames -- fetch historical (and loop)
    //  - when prefix2 is empty:
    //     - if prefix1 is provided and is frame-level and fetch-frame toggle ON -- fetch single frame
    //     - if prefix1 is provided and is frame-level and fetch-frame toggle OFF -- fetch historical
    //          starting from the frame provided
    //     - if prefix1 is provided and is NOT frame-level -- fetch live stream
    
    if (fetchFrameOn)
    {
        if (NameComponents::extractInfo(prefix, prefixInfo) && prefixHasFrameLevelInfo(prefixInfo))
            fetchFrame(prefixInfo);
        else
            errorString_ = "Invalid frame-level prefix provided";
    }
    else
    {
        if (prefix.size() > 0 && endPrefix.size() > 0)
        {
            // fetching historical
            if (NameComponents::extractInfo(prefix, prefixInfo) && prefixHasFrameLevelInfo(prefixInfo) &&
                NameComponents::extractInfo(endPrefix, endPrefixInfo) && prefixHasFrameLevelInfo(endPrefixInfo) &&
                prefixInfo.class_ == SampleClass::Key &&
                endPrefixInfo.class_ == SampleClass::Key)
                createRemoteStreamHistorical(prefixInfo, endPrefixInfo);
            else
                errorString_ = "Fetching historical data (looped): bad prefix(es) provided (must be frame-level and KEY frame type)";
            
        }
        else if (prefix.size() > 0)
        {
            if (NameComponents::extractInfo(prefix, prefixInfo))
            {
                // check if frame-level info is available
                if (prefixHasFrameLevelInfo(prefixInfo))
                    // we are fetching historical, not looped
                    createRemoteStreamHistorical(prefixInfo);
                else
                { // otherwise -- continue with fetching stream
                    basePrefix = prefixInfo.getPrefix(prefix_filter::Base).toUri();
                    streamName = prefixInfo.streamName_;
                }
            }
        }
    }
    
    LogInfoC << "creating remote stream for prefix: " << prefixInfo.getPrefix(prefix_filter::Base).toUri() << endl;
    createRemoteStream(basePrefix, streamName);
}

void
ndnrtcIn::createRemoteStream(const std::string& basePrefix, const std::string& streamName)
{
    stream_ = boost::make_shared<RemoteVideoStream>(faceProcessor_->getIo(),
                                                    faceProcessor_->getFace(),
                                                    keyChainManager_->instanceKeyChain(),
                                                    basePrefix,
                                                    streamName,
                                                    ((ndnrtcIn::Params*)params_)->lifetime_,
                                                    ((ndnrtcIn::Params*)params_)->jitterSize_);
    stream_->setLogger(logger_);
    dynamic_pointer_cast<RemoteStream>(stream_)->registerObserver(this);
    dynamic_pointer_cast<RemoteVideoStream>(stream_)->start(ndnrtcTOPbase::NdnRtcTrheadName, streamRenderer_.get());
}

void
ndnrtcIn::createRemoteStreamHistorical(const NamespaceInfo& prefixStart, const NamespaceInfo& prefixEnd)
{
    RemoteVideoStream::FetchingRuleSet ruleset;
    
    ruleset.skipDelta_ = false;
    ruleset.loop_ = 0;
    ruleset.speed_ = 1;
    ruleset.pipelineSize_ = 20;
    ruleset.seedKeyNo_ = prefixStart.sampleNo_;
    
    if (prefixHasFrameLevelInfo(prefixEnd))
    {
        LogInfoC << "Fetching historical stream (looped) from "
            << prefixStart.getSuffix(suffix_filter::Sample) << " till "
            << prefixEnd.getSuffix(suffix_filter::Sample)
            << std::endl;
        ruleset.loop_ = (prefixEnd.sampleNo_ - prefixStart.sampleNo_);
    }
    else
        LogInfoC << "Fetching historical stream (non-looped) from "
            << prefixStart.getSuffix(suffix_filter::Sample)
            << std::endl;
    
    stream_ = boost::make_shared<RemoteVideoStream>(faceProcessor_->getIo(),
                                                    faceProcessor_->getFace(),
                                                    keyChainManager_->instanceKeyChain(),
                                                    prefixStart.getPrefix(prefix_filter::Thread).toUri(),
                                                    ((ndnrtcIn::Params*)params_)->jitterSize_);
    stream_->setLogger(logger_);
    dynamic_pointer_cast<RemoteStream>(stream_)->registerObserver(this);
    dynamic_pointer_cast<RemoteVideoStream>(stream_)->start(ndnrtcTOPbase::NdnRtcTrheadName, streamRenderer_.get());
}

void
ndnrtcIn::fetchFrame(const ndnrtc::NamespaceInfo& frameInfo)
{
    LogInfoC << "fetching frame " << frameInfo.getPrefix(prefix_filter::Sample) << std::endl;
    
    boost::shared_ptr<RemoteStreamRenderer> renderer = streamRenderer_;
    frameFetcher_ = boost::make_shared<FrameFetcher>(faceProcessor_->getFace(),
                                                     keyChainManager_->instanceKeyChain());
    frameFetcher_->setLogger(logger_);
    frameFetcher_->fetch(frameInfo.getPrefix(prefix_filter::Sample),
                         [renderer](const boost::shared_ptr<IFrameFetcher>&, int width, int height,
                                    IExternalRenderer::BufferType *bufferType)
                         {
                             return renderer->getFrameBuffer(width, height, bufferType);
                         },
                         [renderer](const boost::shared_ptr<IFrameFetcher>&, const FrameInfo& finfo, int nFramesFetched,
                                 int width, int height, const uint8_t* buffer)
                         {
                             renderer->renderFrame(finfo, width, height, buffer);
                         },
                         [this](const boost::shared_ptr<IFrameFetcher>&, std::string reason)
                         {
                             LogError("") << "failed to retrieve frame: " << reason << std::endl;
                             errorString_ = string("Failed to retrieve frame: "+reason);
                         });
}

void
ndnrtcIn::onNewEvent(const ndnrtc::RemoteStream::Event &event)
{
    // handle new event
    LogInfoC << "new event " << event << endl;
}

//******************************************************************************
const void
RemoteStreamRenderer::readBuffer(function<void(const uint8_t* buffer, const FrameInfo& info)> onBufferAccess)
{
    std::lock_guard<std::mutex> bufferLock(bufferReadMutex_);
    onBufferAccess(frontBuffer_.load()->getFrameBuffer(), frontBuffer_.load()->getInfo());
}

uint8_t*
RemoteStreamRenderer::getFrameBuffer(int width, int height, IExternalRenderer::BufferType* bufferType)
{
    *bufferType = IExternalRenderer::kBGRA;
    return backBuffer_.load()->getFrameBuffer(width, height);
}

void
RemoteStreamRenderer::renderFrame(const FrameInfo& frameInfo, int width, int height,
                                      const uint8_t* buffer)
{
    // swap buffers
    backBuffer_.load()->setInfo(frameInfo);
    {
        // TODO: if we're using mutex in the end, why the heck to bother with atomic variables?
        std::lock_guard<std::mutex> bufferLock(bufferReadMutex_);
        backBuffer_ = frontBuffer_.exchange(backBuffer_);
    }
}

//******************************************************************************
bool prefixHasFrameLevelInfo(const NamespaceInfo& prefixInfo)
{
    return (prefixInfo.streamName_ != "" && prefixInfo.threadName_ != "" &&
            (prefixInfo.class_ == SampleClass::Delta || prefixInfo.class_ == SampleClass::Key));
}
