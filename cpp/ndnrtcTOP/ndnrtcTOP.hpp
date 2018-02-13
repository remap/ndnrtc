// 
// ndnrtcTOP.hpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2016 Regents of the University of California
//

#include "TOP_CPlusPlusBase.h"

#include <stdlib.h>
#include <string>
#include <queue>
#include <functional>

#include <ndnrtc/c-wrapper.h>

class ndnrtcTOP : public TOP_CPlusPlusBase
{
public:
    ndnrtcTOP(const OP_NodeInfo *info);
    virtual ~ndnrtcTOP();

    virtual void		getGeneralInfo(TOP_GeneralInfo *) override;
    virtual bool		getOutputFormat(TOP_OutputFormat*) override;


    virtual void		execute(const TOP_OutputFormatSpecs*,
							OP_Inputs*,
							TOP_Context* context) override;


    virtual int32_t		getNumInfoCHOPChans() override;
    virtual void		getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan *chan) override;

    virtual bool		getInfoDATSize(OP_InfoDATSize *infoSize) override;
    virtual void		getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries *entries) override;

	virtual void		setupParameters(OP_ParameterManager *manager) override;
	virtual void		pulsePressed(const char *name) override;

private:

    // We don't need to store this pointer, but we do for the example.
    // The OP_NodeInfo class store information about the node that's using
    // this instance of the class (like its name).
    const OP_NodeInfo		*myNodeInfo;
    
    // FIFO Queue of callbbacks that will be called from within execute() method.
    // Queue will be executed until empty.
    // Callbacks should follow certain signature
    typedef std::function<void(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *)>
            ExecuteCallback;
    std::queue<ExecuteCallback> executeQueue_;
    
    bool                    ndnrtcInitialized_;
    LocalStreamParams       streamParams_;
    
    // NDN-RTC specific parameters
    static std::string      nfdHostname_, signingIdentity_;
    std::string             basePrefix_, streamName_, threadName_;
    
    bool                    signingOn_, fecOn_, canDropFrames_;
    int                     segmentSizeBytes_, freshnessPeriodMs_;
    int                     encodeWidth_, encodeHeight_;
    int                     targetBitrate_, gopSize_;
    
    // In this example this value will be incremented each time the execute()
    // function is called, then passes back to the TOP 
    int						 myExecuteCount;

	double					 myStep;
    
    void                    checkInputs(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
    void                    initNdnrtcLibrary(const TOP_OutputFormatSpecs*, OP_Inputs*, TOP_Context *);
    void                    deinitNdnrtcLibrary();
    void                    createLocalStream();
    LocalStreamParams       readStreamParams() const;
};
