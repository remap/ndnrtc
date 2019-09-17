//
// publish.hpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#ifndef __publish_hpp__
#define __publish_hpp__

#include <boost/asio.hpp>
#include "../include/stream.hpp"

extern std::string AppLog;
extern bool MustTerminate;

void runPublishing(boost::asio::io_service &io,
                   std::string input,
                   std::string basePrefix,
                   std::string streamName,
                   std::string signingIdentity,
                   const ndnrtc::VideoStream::Settings&,
                   int cacheSize,
                   bool needRvp = false,
                   int nLoops = 1,
                   std::string csv = "",
                   std::string stats = "");

#endif
