//
//  ndnrtcIn.hpp
//  ndnrtcIn
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2018 Regents of the University of California
//

#include "ndnrtcTOP_base.hpp"

#include <ndnrtc/params.hpp>
#include <ndnrtc/interfaces.hpp>
#include <ndnrtc/remote-stream.hpp>

#include <boost/atomic.hpp>

namespace ndnrtc {
    namespace statistics {
        class StatisticsStorage;
    }
}

class RemoteStreamRenderer;

class ndnrtcIn : public ndnrtcTOPbase,
ndnrtc::IRemoteStreamObserver
{
public:
    ndnrtcIn(const OP_NodeInfo *info);
    virtual ~ndnrtcIn();
    
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
    boost::shared_ptr<RemoteStreamRenderer> streamRenderer_;
    
    void                    init() override;
    void                    checkInputs(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *) override;
    
    void                    createRemoteStream(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
//    ndnrtc::RemoteStreamParams  readStreamParams(OP_Inputs*) const;
    void                        allocateFramebuffer(int w, int h);
    
    // IRemoteStreamObserver
    void                    onNewEvent(const ndnrtc::RemoteStream::Event&) override;
    
    // IExternalRenderer
//    uint8_t* getFrameBuffer(int width, int height) override;
//    void renderBGRAFrame(int64_t timestamp, uint frameNo,
//                         int width, int height,
//                         const uint8_t* buffer) override;
};


