//
//  ndnrtc_common.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/7/13
//

#ifndef ndnrtc_ndnrtc_common_h
#define ndnrtc_ndnrtc_common_h

#include <stdexcept>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <vector>
#include <cstring>
#include <list>
#include <memory>
#include <math.h>
#include <queue>

#include "ndnlib.h"
#include "webrtc.h"
#include "simple-log.h"

using namespace std;
using namespace ndn;
using namespace ptr_lib;
using namespace ndnlog;

#define PROVIDER_THREAD_ID 1
#define PIPELINER_THREAD_ID 2
#define PLAYOUT_THREAD_ID 3
#define ASSEMBLING_THREAD_ID 4
#define AUDIO_THREAD_ID 5

#define DEPRECATED __attribute__ ((deprecated))

#define RESULT_GOOD(res) (res >= RESULT_OK)
#define RESULT_NOT_OK(res) (res < RESULT_OK)
#define RESULT_FAIL(res) (res <= RESULT_ERR)
#define RESULT_NOT_FAIL(res) (res > RESULT_ERR)
#define RESULT_WARNING(res) (res <= RESULT_WARN && res > RESULT_ERR)

#define RESULT_OK 0
#define RESULT_ERR -100
#define RESULT_WARN -1

// ndn library error
#define ERR_NDNLIB  -101

#endif
