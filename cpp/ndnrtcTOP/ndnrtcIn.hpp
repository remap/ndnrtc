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
    
    class FrameFetcher;
}

class RemoteStreamRenderer;

class ndnrtcIn : public ndnrtcTOPbase, ndnrtc::IRemoteStreamObserver
{
public:
    typedef struct _Params : ndnrtcTOPbase::Params {
        std::string startPrefix_, endPrefix_;
        int jitterSize_, lifetime_;
    } Params;

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
    ndnrtc::FrameInfo       receivedFrameInfo_;
    boost::shared_ptr<ndnrtc::FrameFetcher> frameFetcher_;
    int                     state_;

    void                    initStream() override;
    std::set<std::string>   checkInputs(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *) override;
    
    void                    startFetch(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
    void                    createRemoteStream(const std::string&, const std::string&);
    void                    createRemoteStreamHistorical(const ndnrtc::NamespaceInfo& prefixStart,
                                                         const ndnrtc::NamespaceInfo& ni = ndnrtc::NamespaceInfo());
    void                    fetchFrame(const ndnrtc::NamespaceInfo&);

    void                    allocateFramebuffer(int w, int h);
    
    // IRemoteStreamObserver
    void                    onNewEvent(const ndnrtc::RemoteStream::Event&) override;
};


