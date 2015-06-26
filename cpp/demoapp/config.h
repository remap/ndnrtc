//
//  config.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/10/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
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
