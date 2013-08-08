//
//  ndnrtc_common.h
//  ndnrtc
//
//  Created by Peter Gusev on 8/7/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef ndnrtc_ndnrtc_common_h
#define ndnrtc_ndnrtc_common_h

//#define DEBUG
#define NDN_LOGGING
//#define NDN_DETAILED
#define NDN_TRACE
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_DEBUG

#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <iostream>

#include "nsCOMptr.h"

#include "ndnlib.h"
#include "simple-log.h"

using namespace std;
using namespace ndn;
using namespace ptr_lib;
using namespace ndnlog;


#endif
