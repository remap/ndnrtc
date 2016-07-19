// 
// test-packet-publisher.cc
//
//  Created by Peter Gusev on 12 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <webrtc/common_video/libyuv/include/webrtc_libyuv.h>
#include <boost/assign.hpp>
#include <boost/asio.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.h"
#include "src/packet-publisher.h"
#include "mock-objects/ndn-cpp-mock.h"
#include "frame-data.h"

// include .cpp in order to instantiate class with mock objects
#include "src/packet-publisher.cpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace ndnrtc::statistics;
using namespace ndn;

typedef _PublisherSettings<MockNdnKeyChain, MockNdnMemoryCache> MockSettings;
typedef std::vector<boost::shared_ptr<const ndn::MemoryContentCache::PendingInterest> > PendingInterests;

static uint8_t DEFAULT_RSA_PUBLIC_KEY_DER[] = {
  0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
  0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
  0x00, 0xb8, 0x09, 0xa7, 0x59, 0x82, 0x84, 0xec, 0x4f, 0x06, 0xfa, 0x1c, 0xb2, 0xe1, 0x38, 0x93,
  0x53, 0xbb, 0x7d, 0xd4, 0xac, 0x88, 0x1a, 0xf8, 0x25, 0x11, 0xe4, 0xfa, 0x1d, 0x61, 0x24, 0x5b,
  0x82, 0xca, 0xcd, 0x72, 0xce, 0xdb, 0x66, 0xb5, 0x8d, 0x54, 0xbd, 0xfb, 0x23, 0xfd, 0xe8, 0x8e,
  0xaf, 0xa7, 0xb3, 0x79, 0xbe, 0x94, 0xb5, 0xb7, 0xba, 0x17, 0xb6, 0x05, 0xae, 0xce, 0x43, 0xbe,
  0x3b, 0xce, 0x6e, 0xea, 0x07, 0xdb, 0xbf, 0x0a, 0x7e, 0xeb, 0xbc, 0xc9, 0x7b, 0x62, 0x3c, 0xf5,
  0xe1, 0xce, 0xe1, 0xd9, 0x8d, 0x9c, 0xfe, 0x1f, 0xc7, 0xf8, 0xfb, 0x59, 0xc0, 0x94, 0x0b, 0x2c,
  0xd9, 0x7d, 0xbc, 0x96, 0xeb, 0xb8, 0x79, 0x22, 0x8a, 0x2e, 0xa0, 0x12, 0x1d, 0x42, 0x07, 0xb6,
  0x5d, 0xdb, 0xe1, 0xf6, 0xb1, 0x5d, 0x7b, 0x1f, 0x54, 0x52, 0x1c, 0xa3, 0x11, 0x9b, 0xf9, 0xeb,
  0xbe, 0xb3, 0x95, 0xca, 0xa5, 0x87, 0x3f, 0x31, 0x18, 0x1a, 0xc9, 0x99, 0x01, 0xec, 0xaa, 0x90,
  0xfd, 0x8a, 0x36, 0x35, 0x5e, 0x12, 0x81, 0xbe, 0x84, 0x88, 0xa1, 0x0d, 0x19, 0x2a, 0x4a, 0x66,
  0xc1, 0x59, 0x3c, 0x41, 0x83, 0x3d, 0x3d, 0xb8, 0xd4, 0xab, 0x34, 0x90, 0x06, 0x3e, 0x1a, 0x61,
  0x74, 0xbe, 0x04, 0xf5, 0x7a, 0x69, 0x1b, 0x9d, 0x56, 0xfc, 0x83, 0xb7, 0x60, 0xc1, 0x5e, 0x9d,
  0x85, 0x34, 0xfd, 0x02, 0x1a, 0xba, 0x2c, 0x09, 0x72, 0xa7, 0x4a, 0x5e, 0x18, 0xbf, 0xc0, 0x58,
  0xa7, 0x49, 0x34, 0x46, 0x61, 0x59, 0x0e, 0xe2, 0x6e, 0x9e, 0xd2, 0xdb, 0xfd, 0x72, 0x2f, 0x3c,
  0x47, 0xcc, 0x5f, 0x99, 0x62, 0xee, 0x0d, 0xf3, 0x1f, 0x30, 0x25, 0x20, 0x92, 0x15, 0x4b, 0x04,
  0xfe, 0x15, 0x19, 0x1d, 0xdc, 0x7e, 0x5c, 0x10, 0x21, 0x52, 0x21, 0x91, 0x54, 0x60, 0x8b, 0x92,
  0x41, 0x02, 0x03, 0x01, 0x00, 0x01
};

static uint8_t DEFAULT_RSA_PRIVATE_KEY_DER[] = {
  0x30, 0x82, 0x04, 0xa5, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00, 0xb8, 0x09, 0xa7, 0x59,
  0x82, 0x84, 0xec, 0x4f, 0x06, 0xfa, 0x1c, 0xb2, 0xe1, 0x38, 0x93, 0x53, 0xbb, 0x7d, 0xd4, 0xac,
  0x88, 0x1a, 0xf8, 0x25, 0x11, 0xe4, 0xfa, 0x1d, 0x61, 0x24, 0x5b, 0x82, 0xca, 0xcd, 0x72, 0xce,
  0xdb, 0x66, 0xb5, 0x8d, 0x54, 0xbd, 0xfb, 0x23, 0xfd, 0xe8, 0x8e, 0xaf, 0xa7, 0xb3, 0x79, 0xbe,
  0x94, 0xb5, 0xb7, 0xba, 0x17, 0xb6, 0x05, 0xae, 0xce, 0x43, 0xbe, 0x3b, 0xce, 0x6e, 0xea, 0x07,
  0xdb, 0xbf, 0x0a, 0x7e, 0xeb, 0xbc, 0xc9, 0x7b, 0x62, 0x3c, 0xf5, 0xe1, 0xce, 0xe1, 0xd9, 0x8d,
  0x9c, 0xfe, 0x1f, 0xc7, 0xf8, 0xfb, 0x59, 0xc0, 0x94, 0x0b, 0x2c, 0xd9, 0x7d, 0xbc, 0x96, 0xeb,
  0xb8, 0x79, 0x22, 0x8a, 0x2e, 0xa0, 0x12, 0x1d, 0x42, 0x07, 0xb6, 0x5d, 0xdb, 0xe1, 0xf6, 0xb1,
  0x5d, 0x7b, 0x1f, 0x54, 0x52, 0x1c, 0xa3, 0x11, 0x9b, 0xf9, 0xeb, 0xbe, 0xb3, 0x95, 0xca, 0xa5,
  0x87, 0x3f, 0x31, 0x18, 0x1a, 0xc9, 0x99, 0x01, 0xec, 0xaa, 0x90, 0xfd, 0x8a, 0x36, 0x35, 0x5e,
  0x12, 0x81, 0xbe, 0x84, 0x88, 0xa1, 0x0d, 0x19, 0x2a, 0x4a, 0x66, 0xc1, 0x59, 0x3c, 0x41, 0x83,
  0x3d, 0x3d, 0xb8, 0xd4, 0xab, 0x34, 0x90, 0x06, 0x3e, 0x1a, 0x61, 0x74, 0xbe, 0x04, 0xf5, 0x7a,
  0x69, 0x1b, 0x9d, 0x56, 0xfc, 0x83, 0xb7, 0x60, 0xc1, 0x5e, 0x9d, 0x85, 0x34, 0xfd, 0x02, 0x1a,
  0xba, 0x2c, 0x09, 0x72, 0xa7, 0x4a, 0x5e, 0x18, 0xbf, 0xc0, 0x58, 0xa7, 0x49, 0x34, 0x46, 0x61,
  0x59, 0x0e, 0xe2, 0x6e, 0x9e, 0xd2, 0xdb, 0xfd, 0x72, 0x2f, 0x3c, 0x47, 0xcc, 0x5f, 0x99, 0x62,
  0xee, 0x0d, 0xf3, 0x1f, 0x30, 0x25, 0x20, 0x92, 0x15, 0x4b, 0x04, 0xfe, 0x15, 0x19, 0x1d, 0xdc,
  0x7e, 0x5c, 0x10, 0x21, 0x52, 0x21, 0x91, 0x54, 0x60, 0x8b, 0x92, 0x41, 0x02, 0x03, 0x01, 0x00,
  0x01, 0x02, 0x82, 0x01, 0x01, 0x00, 0x8a, 0x05, 0xfb, 0x73, 0x7f, 0x16, 0xaf, 0x9f, 0xa9, 0x4c,
  0xe5, 0x3f, 0x26, 0xf8, 0x66, 0x4d, 0xd2, 0xfc, 0xd1, 0x06, 0xc0, 0x60, 0xf1, 0x9f, 0xe3, 0xa6,
  0xc6, 0x0a, 0x48, 0xb3, 0x9a, 0xca, 0x21, 0xcd, 0x29, 0x80, 0x88, 0x3d, 0xa4, 0x85, 0xa5, 0x7b,
  0x82, 0x21, 0x81, 0x28, 0xeb, 0xf2, 0x43, 0x24, 0xb0, 0x76, 0xc5, 0x52, 0xef, 0xc2, 0xea, 0x4b,
  0x82, 0x41, 0x92, 0xc2, 0x6d, 0xa6, 0xae, 0xf0, 0xb2, 0x26, 0x48, 0xa1, 0x23, 0x7f, 0x02, 0xcf,
  0xa8, 0x90, 0x17, 0xa2, 0x3e, 0x8a, 0x26, 0xbd, 0x6d, 0x8a, 0xee, 0xa6, 0x0c, 0x31, 0xce, 0xc2,
  0xbb, 0x92, 0x59, 0xb5, 0x73, 0xe2, 0x7d, 0x91, 0x75, 0xe2, 0xbd, 0x8c, 0x63, 0xe2, 0x1c, 0x8b,
  0xc2, 0x6a, 0x1c, 0xfe, 0x69, 0xc0, 0x44, 0xcb, 0x58, 0x57, 0xb7, 0x13, 0x42, 0xf0, 0xdb, 0x50,
  0x4c, 0xe0, 0x45, 0x09, 0x8f, 0xca, 0x45, 0x8a, 0x06, 0xfe, 0x98, 0xd1, 0x22, 0xf5, 0x5a, 0x9a,
  0xdf, 0x89, 0x17, 0xca, 0x20, 0xcc, 0x12, 0xa9, 0x09, 0x3d, 0xd5, 0xf7, 0xe3, 0xeb, 0x08, 0x4a,
  0xc4, 0x12, 0xc0, 0xb9, 0x47, 0x6c, 0x79, 0x50, 0x66, 0xa3, 0xf8, 0xaf, 0x2c, 0xfa, 0xb4, 0x6b,
  0xec, 0x03, 0xad, 0xcb, 0xda, 0x24, 0x0c, 0x52, 0x07, 0x87, 0x88, 0xc0, 0x21, 0xf3, 0x02, 0xe8,
  0x24, 0x44, 0x0f, 0xcd, 0xa0, 0xad, 0x2f, 0x1b, 0x79, 0xab, 0x6b, 0x49, 0x4a, 0xe6, 0x3b, 0xd0,
  0xad, 0xc3, 0x48, 0xb9, 0xf7, 0xf1, 0x34, 0x09, 0xeb, 0x7a, 0xc0, 0xd5, 0x0d, 0x39, 0xd8, 0x45,
  0xce, 0x36, 0x7a, 0xd8, 0xde, 0x3c, 0xb0, 0x21, 0x96, 0x97, 0x8a, 0xff, 0x8b, 0x23, 0x60, 0x4f,
  0xf0, 0x3d, 0xd7, 0x8f, 0xf3, 0x2c, 0xcb, 0x1d, 0x48, 0x3f, 0x86, 0xc4, 0xa9, 0x00, 0xf2, 0x23,
  0x2d, 0x72, 0x4d, 0x66, 0xa5, 0x01, 0x02, 0x81, 0x81, 0x00, 0xdc, 0x4f, 0x99, 0x44, 0x0d, 0x7f,
  0x59, 0x46, 0x1e, 0x8f, 0xe7, 0x2d, 0x8d, 0xdd, 0x54, 0xc0, 0xf7, 0xfa, 0x46, 0x0d, 0x9d, 0x35,
  0x03, 0xf1, 0x7c, 0x12, 0xf3, 0x5a, 0x9d, 0x83, 0xcf, 0xdd, 0x37, 0x21, 0x7c, 0xb7, 0xee, 0xc3,
  0x39, 0xd2, 0x75, 0x8f, 0xb2, 0x2d, 0x6f, 0xec, 0xc6, 0x03, 0x55, 0xd7, 0x00, 0x67, 0xd3, 0x9b,
  0xa2, 0x68, 0x50, 0x6f, 0x9e, 0x28, 0xa4, 0x76, 0x39, 0x2b, 0xb2, 0x65, 0xcc, 0x72, 0x82, 0x93,
  0xa0, 0xcf, 0x10, 0x05, 0x6a, 0x75, 0xca, 0x85, 0x35, 0x99, 0xb0, 0xa6, 0xc6, 0xef, 0x4c, 0x4d,
  0x99, 0x7d, 0x2c, 0x38, 0x01, 0x21, 0xb5, 0x31, 0xac, 0x80, 0x54, 0xc4, 0x18, 0x4b, 0xfd, 0xef,
  0xb3, 0x30, 0x22, 0x51, 0x5a, 0xea, 0x7d, 0x9b, 0xb2, 0x9d, 0xcb, 0xba, 0x3f, 0xc0, 0x1a, 0x6b,
  0xcd, 0xb0, 0xe6, 0x2f, 0x04, 0x33, 0xd7, 0x3a, 0x49, 0x71, 0x02, 0x81, 0x81, 0x00, 0xd5, 0xd9,
  0xc9, 0x70, 0x1a, 0x13, 0xb3, 0x39, 0x24, 0x02, 0xee, 0xb0, 0xbb, 0x84, 0x17, 0x12, 0xc6, 0xbd,
  0x65, 0x73, 0xe9, 0x34, 0x5d, 0x43, 0xff, 0xdc, 0xf8, 0x55, 0xaf, 0x2a, 0xb9, 0xe1, 0xfa, 0x71,
  0x65, 0x4e, 0x50, 0x0f, 0xa4, 0x3b, 0xe5, 0x68, 0xf2, 0x49, 0x71, 0xaf, 0x15, 0x88, 0xd7, 0xaf,
  0xc4, 0x9d, 0x94, 0x84, 0x6b, 0x5b, 0x10, 0xd5, 0xc0, 0xaa, 0x0c, 0x13, 0x62, 0x99, 0xc0, 0x8b,
  0xfc, 0x90, 0x0f, 0x87, 0x40, 0x4d, 0x58, 0x88, 0xbd, 0xe2, 0xba, 0x3e, 0x7e, 0x2d, 0xd7, 0x69,
  0xa9, 0x3c, 0x09, 0x64, 0x31, 0xb6, 0xcc, 0x4d, 0x1f, 0x23, 0xb6, 0x9e, 0x65, 0xd6, 0x81, 0xdc,
  0x85, 0xcc, 0x1e, 0xf1, 0x0b, 0x84, 0x38, 0xab, 0x93, 0x5f, 0x9f, 0x92, 0x4e, 0x93, 0x46, 0x95,
  0x6b, 0x3e, 0xb6, 0xc3, 0x1b, 0xd7, 0x69, 0xa1, 0x0a, 0x97, 0x37, 0x78, 0xed, 0xd1, 0x02, 0x81,
  0x80, 0x33, 0x18, 0xc3, 0x13, 0x65, 0x8e, 0x03, 0xc6, 0x9f, 0x90, 0x00, 0xae, 0x30, 0x19, 0x05,
  0x6f, 0x3c, 0x14, 0x6f, 0xea, 0xf8, 0x6b, 0x33, 0x5e, 0xee, 0xc7, 0xf6, 0x69, 0x2d, 0xdf, 0x44,
  0x76, 0xaa, 0x32, 0xba, 0x1a, 0x6e, 0xe6, 0x18, 0xa3, 0x17, 0x61, 0x1c, 0x92, 0x2d, 0x43, 0x5d,
  0x29, 0xa8, 0xdf, 0x14, 0xd8, 0xff, 0xdb, 0x38, 0xef, 0xb8, 0xb8, 0x2a, 0x96, 0x82, 0x8e, 0x68,
  0xf4, 0x19, 0x8c, 0x42, 0xbe, 0xcc, 0x4a, 0x31, 0x21, 0xd5, 0x35, 0x6c, 0x5b, 0xa5, 0x7c, 0xff,
  0xd1, 0x85, 0x87, 0x28, 0xdc, 0x97, 0x75, 0xe8, 0x03, 0x80, 0x1d, 0xfd, 0x25, 0x34, 0x41, 0x31,
  0x21, 0x12, 0x87, 0xe8, 0x9a, 0xb7, 0x6a, 0xc0, 0xc4, 0x89, 0x31, 0x15, 0x45, 0x0d, 0x9c, 0xee,
  0xf0, 0x6a, 0x2f, 0xe8, 0x59, 0x45, 0xc7, 0x7b, 0x0d, 0x6c, 0x55, 0xbb, 0x43, 0xca, 0xc7, 0x5a,
  0x01, 0x02, 0x81, 0x81, 0x00, 0xab, 0xf4, 0xd5, 0xcf, 0x78, 0x88, 0x82, 0xc2, 0xdd, 0xbc, 0x25,
  0xe6, 0xa2, 0xc1, 0xd2, 0x33, 0xdc, 0xef, 0x0a, 0x97, 0x2b, 0xdc, 0x59, 0x6a, 0x86, 0x61, 0x4e,
  0xa6, 0xc7, 0x95, 0x99, 0xa6, 0xa6, 0x55, 0x6c, 0x5a, 0x8e, 0x72, 0x25, 0x63, 0xac, 0x52, 0xb9,
  0x10, 0x69, 0x83, 0x99, 0xd3, 0x51, 0x6c, 0x1a, 0xb3, 0x83, 0x6a, 0xff, 0x50, 0x58, 0xb7, 0x28,
  0x97, 0x13, 0xe2, 0xba, 0x94, 0x5b, 0x89, 0xb4, 0xea, 0xba, 0x31, 0xcd, 0x78, 0xe4, 0x4a, 0x00,
  0x36, 0x42, 0x00, 0x62, 0x41, 0xc6, 0x47, 0x46, 0x37, 0xea, 0x6d, 0x50, 0xb4, 0x66, 0x8f, 0x55,
  0x0c, 0xc8, 0x99, 0x91, 0xd5, 0xec, 0xd2, 0x40, 0x1c, 0x24, 0x7d, 0x3a, 0xff, 0x74, 0xfa, 0x32,
  0x24, 0xe0, 0x11, 0x2b, 0x71, 0xad, 0x7e, 0x14, 0xa0, 0x77, 0x21, 0x68, 0x4f, 0xcc, 0xb6, 0x1b,
  0xe8, 0x00, 0x49, 0x13, 0x21, 0x02, 0x81, 0x81, 0x00, 0xb6, 0x18, 0x73, 0x59, 0x2c, 0x4f, 0x92,
  0xac, 0xa2, 0x2e, 0x5f, 0xb6, 0xbe, 0x78, 0x5d, 0x47, 0x71, 0x04, 0x92, 0xf0, 0xd7, 0xe8, 0xc5,
  0x7a, 0x84, 0x6b, 0xb8, 0xb4, 0x30, 0x1f, 0xd8, 0x0d, 0x58, 0xd0, 0x64, 0x80, 0xa7, 0x21, 0x1a,
  0x48, 0x00, 0x37, 0xd6, 0x19, 0x71, 0xbb, 0x91, 0x20, 0x9d, 0xe2, 0xc3, 0xec, 0xdb, 0x36, 0x1c,
  0xca, 0x48, 0x7d, 0x03, 0x32, 0x74, 0x1e, 0x65, 0x73, 0x02, 0x90, 0x73, 0xd8, 0x3f, 0xb5, 0x52,
  0x35, 0x79, 0x1c, 0xee, 0x93, 0xa3, 0x32, 0x8b, 0xed, 0x89, 0x98, 0xf1, 0x0c, 0xd8, 0x12, 0xf2,
  0x89, 0x7f, 0x32, 0x23, 0xec, 0x67, 0x66, 0x52, 0x83, 0x89, 0x99, 0x5e, 0x42, 0x2b, 0x42, 0x4b,
  0x84, 0x50, 0x1b, 0x3e, 0x47, 0x6d, 0x74, 0xfb, 0xd1, 0xa6, 0x10, 0x20, 0x6c, 0x6e, 0xbe, 0x44,
  0x3f, 0xb9, 0xfe, 0xbc, 0x8d, 0xda, 0xcb, 0xea, 0x8f
};

boost::shared_ptr<KeyChain> memoryKeyChain(const std::string name)
{
    boost::shared_ptr<MemoryIdentityStorage> identityStorage(new MemoryIdentityStorage());
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(new MemoryPrivateKeyStorage());
    boost::shared_ptr<KeyChain> keyChain(boost::make_shared<KeyChain>
      (boost::make_shared<IdentityManager>(identityStorage, privateKeyStorage),
       boost::make_shared<SelfVerifyPolicyManager>(identityStorage.get())));

    // Initialize the storage.
    Name keyName(name+"/DSK-123");
    Name certificateName = keyName.getSubName(0, keyName.size() - 1).append("KEY").append
           (keyName[-1]).append("ID-CERT").append("0");
    identityStorage->addKey(keyName, KEY_TYPE_RSA, Blob(DEFAULT_RSA_PUBLIC_KEY_DER, sizeof(DEFAULT_RSA_PUBLIC_KEY_DER)));
    privateKeyStorage->setKeyPairForKeyName
      (keyName, KEY_TYPE_RSA, DEFAULT_RSA_PUBLIC_KEY_DER,
       sizeof(DEFAULT_RSA_PUBLIC_KEY_DER), DEFAULT_RSA_PRIVATE_KEY_DER,
       sizeof(DEFAULT_RSA_PRIVATE_KEY_DER));

    return keyChain;
}

TEST(TestPacketPublisher, TestPublishVideoFrame)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
	PendingInterests pendingInterests;

	uint32_t nonce = 1234;
	interest->setNonce(Blob((uint8_t*)&nonce, sizeof(nonce)));
	pendingInterests.push_back(pendingInterest);

	MockNdnKeyChain keyChain;
	MockNdnMemoryCache memoryCache;
	MockSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	settings.keyChain_ = &keyChain;
	settings.memoryCache_ = &memoryCache;
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
	boost::function<void(const Name&, PendingInterests&)> mockGetPendingInterests = 
	[&pendingInterests](const Name& name, PendingInterests& interests){
		interests.clear();
		for (auto p:pendingInterests)
			interests.push_back(p);
	};
	std::vector<Data> dataObjects;
	boost::function<void(const Data&)> mockAddData = [&dataObjects, packetName, wireLength, freshness](const Data& data){
		EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
		EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
		dataObjects.push_back(data);
		
		#if ADD_CRC
		// check CRC value
		NetworkData nd(*data.getContent());
		uint64_t crcValue = data.getName()[-1].toNumber();
		EXPECT_EQ(crcValue, nd.getCrcValue());
		#endif
	};

	EXPECT_CALL(memoryCache, getStorePendingInterest())
        .Times(0);
	EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
		.Times(0);
	EXPECT_CALL(keyChain, sign(_))
		.Times(AtLeast(1));
	EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockGetPendingInterests));
	EXPECT_CALL(memoryCache, add(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockAddData));

	PacketPublisher<VideoFrameSegment, MockSettings> publisher(settings);

	{
		CommonHeader hdr;
		hdr.sampleRate_ = 24.7;
		hdr.publishTimestampMs_ = 488589553;
		hdr.publishUnixTimestampMs_ = 1460488589;

		size_t frameLen = 4300;
		int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
		uint8_t *buffer = (uint8_t*)malloc(frameLen);
		for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

		webrtc::EncodedImage frame(buffer, frameLen, size);
		frame._encodedWidth = 640;
		frame._encodedHeight = 480;
		frame._timeStamp = 1460488589;
		frame.capture_time_ms_ = 1460488569;
		frame._frameType = webrtc::kKeyFrame;
		frame._completeFrame = true;

		VideoFramePacket vp(frame);
		std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

		vp.setSyncList(syncList);
		vp.setHeader(hdr);

		{ // test one pending interest
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;

			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
			EXPECT_EQ(pendingInterest->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
			EXPECT_EQ(nonce, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
		{ // test without passing a header
			dataObjects.clear();
			
			size_t nSlices = publisher.publish(packetName, vp);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
		{ // test two pending interests
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;

			dataObjects.clear();
			boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest2 = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
			pendingInterests.push_back(pendingInterest2);

			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest2->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
			EXPECT_EQ(pendingInterest2->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
			EXPECT_EQ(nonce, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
		{ // test no pending interests
			dataObjects.clear();
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;

			pendingInterests.clear(); 

			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			EXPECT_EQ(0, segHdr.generationDelayMs_);
			EXPECT_EQ(0, segHdr.interestArrivalMs_);
			EXPECT_EQ(0, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
	}
}

TEST(TestPacketPublisher, TestPublishVideoParity)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
	PendingInterests pendingInterests;

	uint32_t nonce = 1234;
	interest->setNonce(Blob((uint8_t*)&nonce, sizeof(nonce)));
	pendingInterests.push_back(pendingInterest);

	MockNdnKeyChain keyChain;
	MockNdnMemoryCache memoryCache;
	MockSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	settings.keyChain_ = &keyChain;
	settings.memoryCache_ = &memoryCache;
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

	Name filter("/testpacket");
	Name packetName(filter);
	packetName.append("1");

	OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
	boost::function<void(const Name&, PendingInterests&)> mockGetPendingInterests = 
	[&pendingInterests](const Name& name, PendingInterests& interests){
		interests.clear();
		for (auto p:pendingInterests)
			interests.push_back(p);
	};
	std::vector<Data> dataObjects;
	boost::function<void(const Data&)> mockAddData = [&dataObjects, packetName, wireLength, freshness](const Data& data){
		EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
		EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
		EXPECT_EQ(CommonSegment::wireLength(VideoFrameSegment::payloadLength(1000)), data.getContent().size());
		dataObjects.push_back(data);
	};

	EXPECT_CALL(memoryCache, getStorePendingInterest())
		.Times(0);
	EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
		.Times(0); // no filter provided
	EXPECT_CALL(keyChain, sign(_))
		.Times(AtLeast(1));
	EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockGetPendingInterests));
	EXPECT_CALL(memoryCache, add(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockAddData));

	PacketPublisher<CommonSegment, MockSettings> publisher(settings);

	{
		CommonHeader hdr;
		hdr.sampleRate_ = 24.7;
		hdr.publishTimestampMs_ = 488589553;
		hdr.publishUnixTimestampMs_ = 1460488589;

		size_t frameLen = 4300;
		int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
		uint8_t *buffer = (uint8_t*)malloc(frameLen);
		for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;

		webrtc::EncodedImage frame(buffer, frameLen, size);
		frame._encodedWidth = 640;
		frame._encodedHeight = 480;
		frame._timeStamp = 1460488589;
		frame.capture_time_ms_ = 1460488569;
		frame._frameType = webrtc::kKeyFrame;
		frame._completeFrame = true;

		VideoFramePacket vp(frame);
		std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);

		vp.setSyncList(syncList);
		vp.setHeader(hdr);

		{ // test one pending interest
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			DataSegmentHeader segHdr;

			size_t nSlices = publisher.publish(packetName, *parityData, segHdr);
			EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
			EXPECT_EQ(pendingInterest->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
			EXPECT_EQ(nonce, segHdr.interestNonce_);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
		{ // test without segment header
			dataObjects.clear();
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);

			size_t nSlices = publisher.publish(packetName, *parityData);
			EXPECT_EQ(nSlices, dataObjects.size());
		}
	}
}

TEST(TestPacketPublisher, TestPublishAudioBundle)
{
		Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryContentCache::PendingInterest> pendingInterest = boost::make_shared<MemoryContentCache::PendingInterest>(interest, face);
	PendingInterests pendingInterests;

	uint32_t nonce = 1234;
	interest->setNonce(Blob((uint8_t*)&nonce, sizeof(nonce)));
	pendingInterests.push_back(pendingInterest);

	MockNdnKeyChain keyChain;
	MockNdnMemoryCache memoryCache;
	MockSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	int data_len = 247;
	settings.keyChain_ = &keyChain;
	settings.memoryCache_ = &memoryCache;
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	OnInterestCallback mockCallback(boost::bind(&MockNdnMemoryCache::storePendingInterestCallback, &memoryCache, _1, _2, _3, _4, _5));
	boost::function<void(const Name&, PendingInterests&)> mockGetPendingInterests = 
	[&pendingInterests](const Name& name, PendingInterests& interests){
		interests.clear();
		for (auto p:pendingInterests)
			interests.push_back(p);
	};
	std::vector<Data> dataObjects;
	boost::function<void(const Data&)> mockAddData = [&dataObjects, packetName, wireLength, freshness, data_len](const Data& data){
		EXPECT_EQ(freshness, data.getMetaInfo().getFreshnessPeriod());
		EXPECT_TRUE(packetName.isPrefixOf(data.getName()));
		EXPECT_EQ(CommonSegment::wireLength(AudioBundlePacket::wireLength(1000, data_len)), data.getContent().size());
		dataObjects.push_back(data);
	};

	EXPECT_CALL(memoryCache, getStorePendingInterest())
        .Times(0);
	EXPECT_CALL(memoryCache, setInterestFilter(filter, _))
		.Times(0);
	EXPECT_CALL(keyChain, sign(_))
		.Times(AtLeast(1));
	EXPECT_CALL(memoryCache, getPendingInterestsForName(_, _))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockGetPendingInterests));
	EXPECT_CALL(memoryCache, add(_))
		.Times(AtLeast(1))
		.WillRepeatedly(Invoke(mockAddData));

	PacketPublisher<CommonSegment, MockSettings> publisher(settings);

	{
		
		std::vector<uint8_t> rtpData;
		for (int i = 0; i < data_len; ++i)
			rtpData.push_back((uint8_t)i);

		int wire_len = wireLength;
		AudioBundlePacket bundlePacket(wireLength);
		AudioBundlePacket::AudioSampleBlob sample({false}, data_len, rtpData.data());

		while (bundlePacket.hasSpace(sample)) bundlePacket << sample;
		
		CommonHeader hdr;
		hdr.sampleRate_ = 24.7;
		hdr.publishTimestampMs_ = 488589553;
		hdr.publishUnixTimestampMs_ = 1460488589;

		bundlePacket.setHeader(hdr);

		DataSegmentHeader segHdr;
		size_t nSlices = publisher.publish(packetName, bundlePacket, segHdr);
		EXPECT_GE(ndn_getNowMilliseconds()-pendingInterest->getTimeoutPeriodStart(), segHdr.generationDelayMs_);
		EXPECT_EQ(pendingInterest->getTimeoutPeriodStart(), segHdr.interestArrivalMs_);
		EXPECT_EQ(nonce, segHdr.interestNonce_);
		EXPECT_EQ(nSlices, dataObjects.size());
		GT_PRINTF("Targeting %d wire length segment, audio bundle wire length is %d bytes "
			"(for sample size %d, bundle fits %d samples total)\n", wire_len, bundlePacket.getLength(), 
			data_len, bundlePacket.getSamplesNum());
	}
}

TEST(TestPacketPublisher, TestBenchmarkSigningSegment1000)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

	PublisherSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	settings.keyChain_ = keyChain.get();
	settings.memoryCache_ = memCache.get();
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	for (int frameLen = 10000; frameLen < 35000; frameLen+=10000)
	{
		VideoPacketPublisher publisher(settings);
		int nFrames = 100;
		unsigned int publishDuration = 0;
		unsigned int totalSlices = 0;
	
		for (int i = 0; i < nFrames; ++i)
		{
			CommonHeader hdr;
			hdr.sampleRate_ = 24.7;
			hdr.publishTimestampMs_ = 488589553;
			hdr.publishUnixTimestampMs_ = 1460488589;
	
			int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
			uint8_t *buffer = (uint8_t*)malloc(frameLen);
			for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;
	
			webrtc::EncodedImage frame(buffer, frameLen, size);
			frame._encodedWidth = 640;
			frame._encodedHeight = 480;
			frame._timeStamp = 1460488589;
			frame.capture_time_ms_ = 1460488569;
			frame._frameType = webrtc::kKeyFrame;
			frame._completeFrame = true;
	
			VideoFramePacket vp(frame);
			std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);
	
			vp.setSyncList(syncList);
			vp.setHeader(hdr);
	
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;
	
			boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
			publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2-t1).count();
			totalSlices += nSlices;
		}
	
		GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f slices per frame average). Average publishing time is %.2fms\n",
			nFrames, frameLen, (double)totalSlices/(double)nFrames, (double)publishDuration/(double)nFrames);
	}
}

TEST(TestPacketPublisher, TestBenchmarkSigningSegment8000)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

	PublisherSettings settings;

	int wireLength = 8000;
	int freshness = 1000;
	settings.keyChain_ = keyChain.get();
	settings.memoryCache_ = memCache.get();
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	for (int frameLen = 10000; frameLen < 35000; frameLen+=10000)
	{
		VideoPacketPublisher publisher(settings);
		int nFrames = 100;
		unsigned int publishDuration = 0;
		unsigned int totalSlices = 0;
	
		for (int i = 0; i < nFrames; ++i)
		{
			CommonHeader hdr;
			hdr.sampleRate_ = 24.7;
			hdr.publishTimestampMs_ = 488589553;
			hdr.publishUnixTimestampMs_ = 1460488589;
	
			int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
			uint8_t *buffer = (uint8_t*)malloc(frameLen);
			for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;
	
			webrtc::EncodedImage frame(buffer, frameLen, size);
			frame._encodedWidth = 640;
			frame._encodedHeight = 480;
			frame._timeStamp = 1460488589;
			frame.capture_time_ms_ = 1460488569;
			frame._frameType = webrtc::kKeyFrame;
			frame._completeFrame = true;
	
			VideoFramePacket vp(frame);
			std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);
	
			vp.setSyncList(syncList);
			vp.setHeader(hdr);
	
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;
	
			boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
			publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2-t1).count();
			totalSlices += nSlices;
		}
	
		GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f slices per frame average). Average publishing time is %.2fms\n",
			nFrames, frameLen, (double)totalSlices/(double)nFrames, (double)publishDuration/(double)nFrames);
	}
}

TEST(TestPacketPublisher, TestBenchmarkNoSigning)
{
	Face face("aleph.ndn.ucla.edu");
	boost::shared_ptr<Interest> interest = boost::make_shared<Interest>("/test/1", 2000);
	boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	boost::shared_ptr<MemoryContentCache> memCache = boost::make_shared<MemoryContentCache>(&face);

	PublisherSettings settings;

	int wireLength = 1000;
	int freshness = 1000;
	settings.keyChain_ = keyChain.get();
	settings.memoryCache_ = memCache.get();
	settings.segmentWireLength_ = wireLength;
	settings.freshnessPeriodMs_ = freshness;
    settings.statStorage_ = StatisticsStorage::createProducerStatistics();
	settings.sign_ = false;

	Name filter("/test"), packetName(filter);
	packetName.append("1");

	for (int frameLen = 10000; frameLen < 35000; frameLen+=10000)
	{
		VideoPacketPublisher publisher(settings);
		int nFrames = 1000;
		unsigned int publishDuration = 0;
		unsigned int totalSlices = 0;
	
		for (int i = 0; i < nFrames; ++i)
		{
			CommonHeader hdr;
			hdr.sampleRate_ = 24.7;
			hdr.publishTimestampMs_ = 488589553;
			hdr.publishUnixTimestampMs_ = 1460488589;
	
			int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
			uint8_t *buffer = (uint8_t*)malloc(frameLen);
			for (int i = 0; i < frameLen; ++i) buffer[i] = i%255;
	
			webrtc::EncodedImage frame(buffer, frameLen, size);
			frame._encodedWidth = 640;
			frame._encodedHeight = 480;
			frame._timeStamp = 1460488589;
			frame.capture_time_ms_ = 1460488569;
			frame._frameType = webrtc::kKeyFrame;
			frame._completeFrame = true;
	
			VideoFramePacket vp(frame);
			std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of ("hi", 341) ("mid", 433) ("low", 432);
	
			vp.setSyncList(syncList);
			vp.setHeader(hdr);
	
			boost::shared_ptr<NetworkData> parityData = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
			VideoFrameSegmentHeader segHdr;
			segHdr.totalSegmentsNum_ = VideoFrameSegment::numSlices(vp, wireLength);
			segHdr.paritySegmentsNum_ = VideoFrameSegment::numSlices(*parityData, wireLength);
			segHdr.playbackNo_ = 100;
			segHdr.pairedSequenceNo_ = 67;
	
			boost::chrono::high_resolution_clock::time_point t1 = boost::chrono::high_resolution_clock::now();
			size_t nSlices = publisher.publish(packetName, vp, segHdr);
			boost::chrono::high_resolution_clock::time_point t2 = boost::chrono::high_resolution_clock::now();
			publishDuration += boost::chrono::duration_cast<boost::chrono::milliseconds>(t2-t1).count();
			totalSlices += nSlices;
		}
	
		GT_PRINTF("Published %d frames. Frame size %d bytes (%.2f slices per frame average). Average publishing time is %.6fms\n",
			nFrames, frameLen, (double)totalSlices/(double)nFrames, (double)publishDuration/(double)nFrames);
	}
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
