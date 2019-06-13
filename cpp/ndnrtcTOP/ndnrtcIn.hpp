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
#include <ndnrtc/name-components.hpp>

#include <boost/atomic.hpp>

namespace ndnrtc {
    namespace statistics {
        class StatisticsStorage;
    }
}

class RemoteStreamRenderer;

class ndnrtcIn : public ndnrtcTOPbase, ndnrtc::IRemoteStreamObserver
{
public:
    typedef struct _Params : ndnrtcTOPbase::Params {
        std::string streamPrefix_;
        int jitterSize_, lifetime_;
    } Params;

    ndnrtcIn(const OP_NodeInfo *info);
    virtual ~ndnrtcIn();
    
    virtual void        getGeneralInfo(TOP_GeneralInfo *, const OP_Inputs*, void *reserved1) override;
    virtual bool        getOutputFormat(TOP_OutputFormat *, const OP_Inputs*, void *reserved1) override;
    
    virtual void        execute(TOP_OutputFormatSpecs*,
                                const OP_Inputs*,
                                TOP_Context* context,
                                void *reserved1) override;

    virtual int32_t     getNumInfoCHOPChans(void *reserved1) override;
    virtual void        getInfoCHOPChan(int32_t index,
                                        OP_InfoCHOPChan *chan,
                                        void *reserved1) override;
    
    virtual bool        getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1) override;
    virtual void        getInfoDATEntries(int32_t index,
                                          int32_t nEntries,
                                          OP_InfoDATEntries *entries,
                                          void *reserved1) override;
    
    virtual void        setupParameters(OP_ParameterManager *manager, void *reserved1) override;
    virtual void        pulsePressed(const char *name, void *reserved1) override;

private:
    boost::shared_ptr<RemoteStreamRenderer> streamRenderer_;
    ndnrtc::FrameInfo  receivedFrameInfo_;
    int state_;

    void                    initStream() override;
    std::set<std::string>   checkInputs(TOP_OutputFormatSpecs*, const OP_Inputs*, TOP_Context *) override;
    
    void                    createRemoteStream(TOP_OutputFormatSpecs*, const OP_Inputs*, TOP_Context *);

    void                        allocateFramebuffer(int w, int h);
    
    // IRemoteStreamObserver
    void                    onNewEvent(const ndnrtc::RemoteStream::Event&) override;
};


