// 
// test-local-media-stream.cc
//
//  Created by Peter Gusev on 07 April 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <stdlib.h>
#include <cstdlib>
#include <ctime>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/policy/no-verify-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <boost/thread.hpp>

#include "gtest/gtest.h"
#include "tests-helpers.h"
#include "src/local-media-stream.h"
#include "include/name-components.h"

// #define ENABLE_LOGGING

using namespace ndnrtc;
using namespace ndn;
using namespace boost;
using namespace boost::chrono;

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

MediaStreamParams getSampleVideoParams()
{
	MediaStreamParams msp("camera");

	msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
	msp.synchronizedStreamName_ = "mic";
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;

	CaptureDeviceParams cdp;
	cdp.deviceId_ = 10;
	msp.captureDevice_ = cdp;

	VideoThreadParams atp("low", sampleVideoCoderParams());
	atp.coderParams_.encodeWidth_ = 640;
	atp.coderParams_.encodeHeight_ = 480;
	msp.addMediaThread(atp);

	VideoThreadParams atp2("mid", sampleVideoCoderParams());
	atp2.coderParams_.encodeWidth_ = 1280;
	atp2.coderParams_.encodeHeight_ = 720;
	msp.addMediaThread(atp2);

	return msp;
}

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

MediaStreamParams getSampleAudioParams()
{
	MediaStreamParams msp("mic");

	msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
	msp.producerParams_.freshnessMs_ = 2000;
	msp.producerParams_.segmentSize_ = 1000;
	
	CaptureDeviceParams cdp;
	cdp.deviceId_ = 0;
	msp.captureDevice_ = cdp;
	msp.addMediaThread(AudioThreadParams("hd", "opus"));
	msp.addMediaThread(AudioThreadParams("sd", "g722"));

	return msp;
}

#if 1
TEST(TestVideoStream, TestCreate)
{
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	ndn::KeyChain keyChain;
	MediaStreamSettings settings(io, getSampleVideoParams());
	settings.face_ = &face;
	settings.keyChain_ = &keyChain;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";
	LocalVideoStream s(streamPrefix, settings);

	ASSERT_EQ(2, s.getThreads().size());
	EXPECT_EQ("low", s.getThreads()[0]);
	EXPECT_EQ("mid", s.getThreads()[1]);
	std::stringstream apiV;
	apiV << "\%FD\%" << std::setw(2) << std::setfill('0') << NameComponents::nameApiVersion();

	EXPECT_EQ(streamPrefix+"/ndnrtc/"+apiV.str()+"/video/camera", s.getPrefix());
}

TEST(TestVideoStream, TestParamsError)
{
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	ndn::KeyChain keyChain;
	MediaStreamSettings settings(io, getSampleVideoParams());
	settings.face_ = &face;
	settings.keyChain_ = &keyChain;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

	settings.params_.getVideoThread(1)->threadName_ = settings.params_.getVideoThread(0)->threadName_;

	EXPECT_ANY_THROW(LocalVideoStream s(streamPrefix, settings));
}

TEST(TestVideoStream, TestAddExistingThread)
{
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	ndn::KeyChain keyChain;
	MediaStreamSettings settings(io, getSampleVideoParams());
	settings.face_ = &face;
	settings.keyChain_ = &keyChain;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

	LocalVideoStream s(streamPrefix, settings);
	EXPECT_ANY_THROW(s.addThread(*settings.params_.getVideoThread(0)));
}

TEST(TestVideoStream, TestCreateWrongMediaSettings)
{
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	ndn::KeyChain keyChain;
	MediaStreamSettings settings(io, getSampleVideoParams());
	settings.face_ = &face;
	settings.keyChain_ = &keyChain;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

	settings.params_.type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeAudio;

	EXPECT_ANY_THROW(LocalVideoStream s(streamPrefix, settings));
}

TEST(TestVideoStream, TestDeleteThreads)
{
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	ndn::KeyChain keyChain;
	MediaStreamSettings settings(io, getSampleVideoParams());
	settings.face_ = &face;
	settings.keyChain_ = &keyChain;
	std::string streamPrefix = "/ndn/edu/ucla/remap/peter/app";

	LocalVideoStream s(streamPrefix, settings);

	EXPECT_EQ(2, s.getThreads().size());
	EXPECT_NO_THROW(s.removeThread("wrong-thread"));
	EXPECT_EQ(2, s.getThreads().size());
	
	s.removeThread(settings.params_.getVideoThread(0)->threadName_);
	EXPECT_EQ(1, s.getThreads().size());

	s.removeThread(settings.params_.getVideoThread(1)->threadName_);
	EXPECT_EQ(0, s.getThreads().size());

	// test feeding frame when there are no threads
	int width = 1280, height = 720;
	int frameSize = width*height*4*sizeof(uint8_t);
	uint8_t *frameBuffer = (uint8_t*)malloc(frameSize);
	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

	EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frameBuffer, frameSize));
}
#if 1
TEST(TestVideoStream, TestPublish)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	int width = 1280, height = 720;
	int frameSize = width*height*4*sizeof(uint8_t);
	uint8_t *frameBuffer = (uint8_t*)malloc(frameSize);
	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

	boost::asio::io_service io;
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	{ // create a scope, to make sure that t.join() will be called after
	  // stream is destroyed
		MediaStreamSettings settings(io, getSampleVideoParams());
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
	s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

		EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frameBuffer, frameSize));

		work.reset();
	}

	t.join();
	free(frameBuffer);
}

TEST(TestVideoStream, TestPublish5Sec)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	int nFrames = 150;
	int width = 1280, height = 720;
	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	std::vector<boost::shared_ptr<uint8_t>> frames;

	uint8_t *frameBuffer = new uint8_t[frameSize];
	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

	frames.push_back(boost::shared_ptr<uint8_t>(frameBuffer));

	for (int f = 1; f < nFrames; ++f)
	{
		// add noise in rectangle in the center of frame
		uint8_t *buf = new uint8_t[frameSize];
		memcpy(buf, frameBuffer, frameSize);

		for (int j = height/3; j < 2*height/3; ++j)
			for (int i = width/3; i < 2*width/3; ++i)
				frameBuffer[i*width+j] = std::rand()%256; // random noise

		frames.push_back(boost::shared_ptr<uint8_t>(buf));
	}

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	{
		MediaStreamSettings settings(io, getSampleVideoParams());
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
	s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
		boost::chrono::duration<int, boost::milli> pubDuration;
		high_resolution_clock::time_point pubStart;
		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			pubStart = high_resolution_clock::now();
			EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frames[i].get(), frameSize));
			pubDuration += duration_cast<milliseconds>( high_resolution_clock::now() - pubStart );
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
	
		double avgFramePubTimeMs = (double)pubDuration.count()/(double)nFrames;
		EXPECT_GE(1000/30, avgFramePubTimeMs);
		GT_PRINTF("publishing took %d milliseconds (avg %.2f ms per frame), %d ms total test\n",
			pubDuration.count(), avgFramePubTimeMs, duration);
		
		work.reset();
	}
	t.join();
}

TEST(TestVideoStream, TestPublish3SecOnFaceThread)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	int nFrames = 30*3;
	int width = 1280, height = 720;
	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	std::vector<boost::shared_ptr<uint8_t>> frames;

	uint8_t *frameBuffer = new uint8_t[frameSize];
	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

	frames.push_back(boost::shared_ptr<uint8_t>(frameBuffer));

	for (int f = 1; f < nFrames; ++f)
	{
		// add noise in rectangle in the center of frame
		uint8_t *buf = new uint8_t[frameSize];
		memcpy(buf, frameBuffer, frameSize);

		for (int j = height/3; j < 2*height/3; ++j)
			for (int i = width/3; i < 2*width/3; ++i)
				frameBuffer[i*width+j] = std::rand()%256; // random noise

		frames.push_back(boost::shared_ptr<uint8_t>(buf));
	}

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	{
		MediaStreamSettings settings(io, getSampleVideoParams());
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
	s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
		boost::chrono::duration<int, boost::milli> pubDuration;
		high_resolution_clock::time_point pubStart;
		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			pubStart = high_resolution_clock::now();
			
			io.dispatch([&frames, i, &s, &width, &height, &frameSize](){
				EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frames[i].get(), frameSize));
			});
	
			pubDuration += duration_cast<milliseconds>( high_resolution_clock::now() - pubStart );
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
	
		double avgFramePubTimeMs = (double)pubDuration.count()/(double)nFrames;
		EXPECT_GE(1000/30, avgFramePubTimeMs);
	
		work.reset();
	}
	t.join();
}
#endif

TEST(TestVideoStream, TestRemoveThreadWhilePublishing)
{
#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif

	int nFrames = 3*30;
	int width = 1280, height = 720;
	std::srand(std::time(0));
	int frameSize = width*height*4*sizeof(uint8_t);
	std::vector<boost::shared_ptr<uint8_t>> frames;

	uint8_t *frameBuffer = new uint8_t[frameSize];
	for (int j = 0; j < height; ++j)
		for (int i = 0; i < width; ++i)
			frameBuffer[i*width+j] = std::rand()%256; // random noise

	frames.push_back(boost::shared_ptr<uint8_t>(frameBuffer));

	for (int f = 1; f < nFrames; ++f)
	{
		// add noise in rectangle in the center of frame
		uint8_t *buf = new uint8_t[frameSize];
		memcpy(buf, frameBuffer, frameSize);

		for (int j = height/3; j < 2*height/3; ++j)
			for (int i = width/3; i < 2*width/3; ++i)
				frameBuffer[i*width+j] = std::rand()%256; // random noise

		frames.push_back(boost::shared_ptr<uint8_t>(buf));
	}

	boost::asio::io_service io;
	boost::asio::deadline_timer runTimer(io);
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(boost::make_shared<MemoryPrivateKeyStorage>());
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);

	{
		MediaStreamSettings settings(io, getSampleVideoParams());
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalVideoStream s(appPrefix, settings);

#ifdef ENABLE_LOGGING
	s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif
		boost::chrono::duration<int, boost::milli> pubDuration;
		high_resolution_clock::time_point pubStart;
		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		for (int i = 0; i < nFrames; ++i)
		{
			runTimer.expires_from_now(boost::posix_time::milliseconds(30));
			pubStart = high_resolution_clock::now();
			
			EXPECT_NO_THROW(s.incomingArgbFrame(width, height, frames[i].get(), frameSize));
			
			if (i == nFrames/2)
				s.removeThread(s.getThreads().front());
	
			pubDuration += duration_cast<milliseconds>( high_resolution_clock::now() - pubStart );
			runTimer.wait();
		}
		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		auto duration = duration_cast<milliseconds>( t2 - t1 ).count();
	
		double avgFramePubTimeMs = (double)pubDuration.count()/(double)nFrames;
		EXPECT_GE(1000/30, avgFramePubTimeMs);
		GT_PRINTF("publishing took %d milliseconds (avg %.2f ms per frame), %d ms total test\n",
			pubDuration.count(), avgFramePubTimeMs, duration);
		
		work.reset();
	}
	t.join();
}
#endif
#if 1
TEST(TestAudioStream, TestCreate)
{
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	MediaStreamSettings settings(io, getSampleAudioParams());
	settings.face_ = &face;
	settings.keyChain_ = keyChain.get();
	LocalAudioStream s(appPrefix, settings);

	EXPECT_FALSE(s.isRunning());
	ASSERT_EQ(2, s.getThreads().size());
	EXPECT_EQ("hd", s.getThreads()[0]);
	EXPECT_EQ("sd", s.getThreads()[1]);
	std::stringstream apiV;
	apiV << "\%FD\%" << std::setw(2) << std::setfill('0') << NameComponents::nameApiVersion();

	EXPECT_EQ(appPrefix+"/ndnrtc/"+apiV.str()+"/audio/mic", s.getPrefix());
}

TEST(TestAudioStream, TestCreateWrongMediaSettings)
{
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	ndn::KeyChain keyChain;
	MediaStreamSettings settings(io, getSampleVideoParams());
	settings.face_ = &face;
	settings.keyChain_ = &keyChain;
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	EXPECT_ANY_THROW(LocalAudioStream(appPrefix, settings));
}

TEST(TestAudioStream, TestAddRemoveThreads)
{
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	MediaStreamSettings settings(io, getSampleAudioParams());
	settings.face_ = &face;
	settings.keyChain_ = keyChain.get();
	LocalAudioStream s(appPrefix, settings);

	ASSERT_EQ(2, s.getThreads().size());
	EXPECT_EQ("hd", s.getThreads()[0]);
	EXPECT_EQ("sd", s.getThreads()[1]);
	std::stringstream apiV;
	apiV << "\%FD\%" << std::setw(2) << std::setfill('0') << NameComponents::nameApiVersion();

	EXPECT_EQ(appPrefix+"/ndnrtc/"+apiV.str()+"/audio/mic", s.getPrefix());

	EXPECT_NO_THROW(s.addThread(AudioThreadParams("sd2")));
	EXPECT_EQ(3, s.getThreads().size());
	EXPECT_ANY_THROW(s.addThread(AudioThreadParams("sd2")));

	EXPECT_NO_THROW(s.removeThread("sd"));
	EXPECT_EQ(2, s.getThreads().size());
	EXPECT_NO_THROW(s.removeThread("hd"));
	EXPECT_EQ(1, s.getThreads().size());
	EXPECT_NO_THROW(s.removeThread("sd2"));
	EXPECT_EQ(0, s.getThreads().size());

	EXPECT_NO_THROW(s.removeThread("sd"));
	EXPECT_EQ(0, s.getThreads().size());
}

TEST(TestAudioStream, TestRun5Sec)
{
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	boost::asio::deadline_timer runTimer(io);
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});


#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	{
		MediaStreamSettings settings(io, getSampleAudioParams());
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalAudioStream s(appPrefix, settings);
		EXPECT_FALSE(s.isRunning());

#ifdef ENABLE_LOGGING
		s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

		runTimer.expires_from_now(boost::posix_time::milliseconds(5000));
		EXPECT_NO_THROW(s.start());
		EXPECT_TRUE(s.isRunning());

		runTimer.wait();
		EXPECT_NO_THROW(s.stop());
		EXPECT_FALSE(s.isRunning());
	}

	work.reset();
	t.join();
}
#endif
#if 0
TEST(TestAudioStream, TestRunAndStopOnDtor)
{
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	boost::asio::deadline_timer runTimer(io);
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	{
		MediaStreamSettings settings(io, getSampleAudioParams());
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalAudioStream s(appPrefix, settings);
		EXPECT_FALSE(s.isRunning());

#ifdef ENABLE_LOGGING
		s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

		runTimer.expires_from_now(boost::posix_time::milliseconds(500));
		EXPECT_NO_THROW(s.start());
		EXPECT_TRUE(s.isRunning());

		runTimer.wait();
	}

	work.reset();
	t.join();
}
#endif
#if 1
TEST(TestAudioStream, TestAddRemoveThreadsOnTheFly)
{
	std::string appPrefix = "/ndn/edu/ucla/remap/peter/app";
	boost::asio::io_service io;
	ndn::Face face("aleph.ndn.ucla.edu");
	shared_ptr<KeyChain> keyChain = memoryKeyChain(appPrefix);
	boost::asio::deadline_timer runTimer(io);
	boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
	boost::thread t([&io](){
		io.run();
	});

#ifdef ENABLE_LOGGING
	ndnlog::new_api::Logger::initAsyncLogging();
	ndnlog::new_api::Logger::getLogger("").setLogLevel(ndnlog::NdnLoggerDetailLevelAll);
#endif
	{
		MediaStreamParams msp("mic");
		msp.producerParams_.segmentSize_ = 1000;
		msp.producerParams_.freshnessMs_ = 1000;
		msp.captureDevice_.deviceId_ = 0;
		msp.type_ = MediaStreamParams::MediaStreamType::MediaStreamTypeAudio;
		msp.addMediaThread(AudioThreadParams("sd"));
		MediaStreamSettings settings(io, msp);
		settings.face_ = &face;
		settings.keyChain_ = keyChain.get();
		LocalAudioStream s(appPrefix, settings);
		EXPECT_FALSE(s.isRunning());

#ifdef ENABLE_LOGGING
		s.setLogger(&ndnlog::new_api::Logger::getLogger(""));
#endif

		runTimer.expires_from_now(boost::posix_time::milliseconds(1000));
		EXPECT_NO_THROW(s.start());
		EXPECT_TRUE(s.isRunning());

		runTimer.wait();
		int n = 10;
		std::srand(std::time(0));

		for (int i = 0; i < n; ++i)
		{
			GT_PRINTF("thread added...\n");
			EXPECT_NO_THROW(s.addThread(AudioThreadParams("hd","opus")));
			EXPECT_TRUE(s.isRunning());
			runTimer.expires_from_now(boost::posix_time::milliseconds(std::rand()%1000+500));
			runTimer.wait();
	
			GT_PRINTF("thread removed %d/%d\n", i+1, n);
			EXPECT_NO_THROW(s.removeThread("hd"));
			EXPECT_TRUE(s.isRunning());
			runTimer.expires_from_now(boost::posix_time::milliseconds(std::rand()%1000+500));
			runTimer.wait();
		}

		EXPECT_NO_THROW(s.removeThread("sd"));
		EXPECT_FALSE(s.isRunning());
	}

	work.reset();
	t.join();
}
#endif

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
