// 
// ndnrtcOut.hpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcTOP_base.hpp"

#include <ndnrtc/params.hpp>
#include <ndnrtc/interfaces.hpp>

namespace ndnrtc {
    namespace statistics {
        class StatisticsStorage;
    }
}

#define USE_ENCODE_RESOLUTION 0 // use encoding resolution as a parameter

class ndnrtcOut : public ndnrtcTOPbase
{
public:
    typedef struct _Params : public ndnrtcTOPbase::Params {
        enum { TargetBitrate, Fec, Signing, FrameDrop, SegmentSize, GopSize };
        
        int targetBitrate_, segmentSize_, gopSize_;
        bool fecEnabled_, signingEnabled_, frameDropAllowed_;
#if USE_ENCODE_RESOLUTION
        int encodeWidth_, encodeHeight_;
#endif
    } Params;
    
    ndnrtcOut(const OP_NodeInfo *info);
    virtual ~ndnrtcOut();
    
    virtual void        getGeneralInfo(TOP_GeneralInfo *, const OP_Inputs*, void* reserved1) override;
    virtual bool        getOutputFormat(TOP_OutputFormat*, const OP_Inputs*, void* reserved1) override;
    
    virtual void        execute(TOP_OutputFormatSpecs*,
                                const OP_Inputs*,
                                TOP_Context* context,
                                void* reserved1) override;

    virtual int32_t     getNumInfoCHOPChans(void* reserved1) override;
    virtual void        getInfoCHOPChan(int32_t index,
                                        OP_InfoCHOPChan *chan,
                                        void* reserved1) override;
    
    virtual bool        getInfoDATSize(OP_InfoDATSize *infoSize, void* reserved1) override;
    virtual void        getInfoDATEntries(int32_t index,
                                          int32_t nEntries,
                                          OP_InfoDATEntries *entries,
                                          void* reserved1) override;
    
    virtual void        setupParameters(OP_ParameterManager *manager, void* reserved1) override;
    virtual void        pulsePressed(const char *name, void* reserved1) override;
    
private:
    int                 publishedFrame_;
    ndnrtc::FrameInfo   publishedFrameInfo_;
    
    int                     incomingFrameBufferSize_, incomingFrameWidth_, incomingFrameHeight_;
    unsigned char*          incomingFrameBuffer_;
    
    void                    init() override;
    void                    initStream() override;
    std::set<std::string>   checkInputs(TOP_OutputFormatSpecs*, const OP_Inputs*, TOP_Context *) override;

    void                    createLocalStream(TOP_OutputFormatSpecs*, const OP_Inputs*, TOP_Context *);
    ndnrtc::MediaStreamParams   readStreamParams(const OP_Inputs*) const;
    void                        allocateIncomingFramebuffer(int w, int h);
};
