//
// fetch.hpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#ifndef __fetch_hpp__
#define __fetch_hpp__

#include <string>

extern std::string AppLog;
extern bool MustTerminate;

namespace boost {
    namespace asio {
        class io_service;
    }
}

namespace ndnrtc {
    class NamespaceInfo;
}

void runFetching(boost::asio::io_service &io,
                 std::string output,
                 const ndnrtc::NamespaceInfo& prefixInfo,
                 long ppSize,
                 long ppStep,
                 long pbcRate,
                 bool useFec,
                 bool needRvp = false,
                 std::string policyFile = "");

#endif
