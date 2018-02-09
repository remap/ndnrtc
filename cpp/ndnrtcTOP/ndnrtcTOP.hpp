// 
// ndnrtcTOP.hpp
//
//  Created by Peter Gusev on 09 February 2018.
//  Copyright 2013-2016 Regents of the University of California
//

#include "TOP_CPlusPlusBase.h"

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

    // In this example this value will be incremented each time the execute()
    // function is called, then passes back to the TOP 
    int						 myExecuteCount;

	double					 myStep;


};
