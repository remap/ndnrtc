// 
// ndnrtcOut.hpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2016 Regents of the University of California
//

#include "ndnrtcTOP_base.hpp"
#include <ndnrtc/c-wrapper.h>

namespace ndnrtc {
    class LocalVideoStream;
    
    namespace statistics {
        class StatisticsStorage;
    }
}

class ndnrtcOut : public ndnrtcTOPbase
{
public:
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
//    ndnrtc::LocalVideoStream*        localStream_;
    
    int                     publbishedFrame_;
    
    int                     incomingFrameBufferSize_, incomingFrameWidth_, incomingFrameHeight_;
    unsigned char*          incomingFrameBuffer_;
    
    void                    checkInputs(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *) override;

    void                    createLocalStream(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
    LocalStreamParams       readStreamParams(OP_Inputs*) const;
    void                    allocateIncomingFramebuffer(int w, int h);
};
