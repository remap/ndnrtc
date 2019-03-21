//
// publish.hpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#ifndef __publish_hpp__
#define __publish_hpp__

#include "../include/stream.hpp"

extern std::string AppLog;
extern bool MustTerminate;

namespace boost {
    namespace asio {
        class io_context;
    }
}

void runPublishing(boost::asio::io_context &io,
                   std::string input,
                   std::string basePrefix,
                   std::string streamName,
                   std::string signingIdentity,
                   const ndnrtc::VideoStream::Settings&,
                   bool needRvp = false,
                   bool isLooped = false);

#endif
