// 
// c-wrapper.cpp
//
// Copyright (c) 2017 Regents of the University of California
// For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "c-wrapper.h"

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <boost/asio.hpp>
#include <boost/thread/lock_guard.hpp>

using namespace ndn;
using namespace ndnrtc;

void ndnrtc_init(const char* hostname, unsigned int portnum, 
	const char* storagePath)
{

}

bool ndnrtc_isConnected()
{

}

void ndnrtc_deinit()
{

}
