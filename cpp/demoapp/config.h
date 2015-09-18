//
//  config.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/10/13.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef ndnrtc_config_h
#define ndnrtc_config_h

#include <string.h>
#include "simple-log.h"
#include "ndnrtc-library.h"

int loadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::ParamsStruct &params,
                       ndnrtc::ParamsStruct &audioParams) DEPRECATED;

int loadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::AppParams &params);
#endif
