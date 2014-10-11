//
//  name-components.cpp
//  libndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "name-components.h"

using namespace ndnrtc;

const std::string NameComponents::NameComponentApp = "ndnrtc";
const std::string NameComponents::NameComponentUser = "user";
const std::string NameComponents::NameComponentSession = "session-info";
const std::string NameComponents::NameComponentBroadcast = "/ndn/broadcast";
const std::string NameComponents::NameComponentDiscovery = "discovery";
const std::string NameComponents::NameComponentUserStreams = "streams";
const std::string NameComponents::NameComponentStreamAccess = "access";
const std::string NameComponents::NameComponentStreamKey = "key";
const std::string NameComponents::NameComponentStreamFramesDelta = "delta";
const std::string NameComponents::NameComponentStreamFramesKey = "key";
const std::string NameComponents::NameComponentStreamInfo = "info";
const std::string NameComponents::NameComponentFrameSegmentData = "data";
const std::string NameComponents::NameComponentFrameSegmentParity = "parity";
const std::string NameComponents::KeyComponent = "DSK-1408";
const std::string NameComponents::CertificateComponent = "ID-CERT/0";
