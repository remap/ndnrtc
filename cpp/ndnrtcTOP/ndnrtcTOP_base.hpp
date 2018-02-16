//
//  ndnrtcTOP_base.hpp
//  ndnrtcTOP
//
//  Created by Peter Gusev on 2/16/18.
//  Copyright © 2018 UCLA REMAP. All rights reserved.
//

#ifndef ndnrtcTOP_base_hpp
#define ndnrtcTOP_base_hpp

#include "TOP_CPlusPlusBase.h"
#include <stdlib.h>
#include <string>
#include <queue>
#include <functional>

namespace ndnrtc {
    class IStream;
    
    namespace statistics {
        class StatisticsStorage;
    }
}


class ndnrtcTOPbase : public TOP_CPlusPlusBase
{
public:
    ndnrtcTOPbase(const OP_NodeInfo *info);
    virtual ~ndnrtcTOPbase();
    
    virtual void        execute(const TOP_OutputFormatSpecs*,
                                OP_Inputs*,
                                TOP_Context* context) override;
    
    virtual int32_t     getNumInfoCHOPChans() override { return 0; }
    virtual void        getInfoCHOPChan(int32_t index,
                                        OP_InfoCHOPChan *chan) override {}
    
    virtual bool        getInfoDATSize(OP_InfoDATSize *infoSize) override;
    virtual void        getInfoDATEntries(int32_t index,
                                          int32_t nEntries,
                                          OP_InfoDATEntries *entries) override;
    
    virtual void        setupParameters(OP_ParameterManager *manager) override;
    virtual void        pulsePressed(const char *name) override;
    
    const char*         getWarningString() override;
    const char*         getErrorString() override;
    
protected:
    const OP_NodeInfo        *myNodeInfo;
    
    // FIFO Queue of callbbacks that will be called from within execute() method.
    // Queue will be executed until empty.
    // Callbacks should follow certain signature
    typedef std::function<void(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *)>
    ExecuteCallback;
    std::queue<ExecuteCallback> executeQueue_;
    
    bool                    ndnrtcInitialized_;
    std::string             errorString_, warningString_;
    
    ndnrtc::IStream*        stream_;
    ndnrtc::statistics::StatisticsStorage*  statStorage_;
    
    void                    initNdnrtcLibrary(const TOP_OutputFormatSpecs*,
                                              OP_Inputs*,
                                              TOP_Context *);
    void                    deinitNdnrtcLibrary();
    
    virtual void            checkInputs(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
    void                    libraryLog(const char* msg);
    std::string             generateName() const;
    void                    readStreamStats();
    std::string             readBasePrefix(OP_Inputs* inputs) const;
};

#endif /* ndnrtcTOP_base_hpp */
