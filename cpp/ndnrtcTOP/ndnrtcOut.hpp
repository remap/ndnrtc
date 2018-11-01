// 
// ndnrtcOut.hpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcTOP_base.hpp"

#include <ndnrtc/params.hpp>

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
    
    virtual void        getGeneralInfo(TOP_GeneralInfo *) override;
    virtual bool        getOutputFormat(TOP_OutputFormat*) override;
    
    virtual void        execute(const TOP_OutputFormatSpecs*,
                                OP_Inputs*,
                                TOP_Context* context) override;

    virtual int32_t     getNumInfoCHOPChans() override;
    virtual void        getInfoCHOPChan(int32_t index,
                                        OP_InfoCHOPChan *chan) override;
    
    virtual bool        getInfoDATSize(OP_InfoDATSize *infoSize) override;
    virtual void        getInfoDATEntries(int32_t index,
                                          int32_t nEntries,
                                          OP_InfoDATEntries *entries) override;
    
    virtual void        setupParameters(OP_ParameterManager *manager) override;
    virtual void        pulsePressed(const char *name) override;
    
private:
    int                 publbishedFrame_;
    
    int                     incomingFrameBufferSize_, incomingFrameWidth_, incomingFrameHeight_;
    unsigned char*          incomingFrameBuffer_;
    
    void                    init() override;
    void                    initStream() override;
    std::set<std::string>   checkInputs(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *) override;

    void                    createLocalStream(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
    ndnrtc::MediaStreamParams   readStreamParams(OP_Inputs*) const;
    void                        allocateIncomingFramebuffer(int w, int h);
};
