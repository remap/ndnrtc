//
// tests-helpers.cc
//
//  Created by Peter Gusev on 05 March 2016.
//  Copyright 2013-2016 Regents of the University of California
//

#include <boost/assign.hpp>
#include <ndn-cpp/interest.hpp>
#include <ndn-cpp/threadsafe-face.hpp>

#include "tests-helpers.hpp"

using namespace std;
using namespace ndnrtc;
using namespace ndn;

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
    0x41, 0x02, 0x03, 0x01, 0x00, 0x01};

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
    0x3f, 0xb9, 0xfe, 0xbc, 0x8d, 0xda, 0xcb, 0xea, 0x8f};

VideoCoderParams sampleVideoCoderParams()
{
    VideoCoderParams vcp;

    vcp.codecFrameRate_ = 30;
    vcp.gop_ = 30;
    vcp.startBitrate_ = 1000;
    vcp.maxBitrate_ = 3000;
    vcp.encodeWidth_ = 1920;
    vcp.encodeHeight_ = 1080;
    vcp.dropFramesOn_ = true;

    return vcp;
}

ClientParams sampleConsumerParams()
{
    ClientParams cp;

    {
        GeneralParams gp;

        gp.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
        gp.logFile_ = "ndnrtc.log";
        gp.logPath_ = "/tmp";
        gp.useFec_ = false;
        gp.useAvSync_ = true;
        gp.host_ = "aleph.ndn.ucla.edu";
        gp.portNum_ = 6363;
        cp.setGeneralParameters(gp);
    }

    {
        ConsumerStreamParams msp1;

        msp1.sessionPrefix_ = "/ndn/edu/ucla/remap/client1";
        msp1.sink_ = {"mic.pcmu", "file", false};
        msp1.threadToFetch_ = "pcmu";
        msp1.streamName_ = "mic";
        msp1.type_ = MediaStreamParams::MediaStreamTypeAudio;
        msp1.synchronizedStreamName_ = "camera";
        msp1.producerParams_.freshness_ = {15, 15, 900};
        msp1.producerParams_.segmentSize_ = 1000;

        ConsumerStreamParams msp2;

        msp2.sessionPrefix_ = "/ndn/edu/ucla/remap/client1";
        msp2.sink_ = {"camera.yuv", "file", false};
        msp2.threadToFetch_ = "low";
        msp2.streamName_ = "camera";
        msp2.type_ = MediaStreamParams::MediaStreamTypeVideo;
        msp2.synchronizedStreamName_ = "mic";
        msp2.producerParams_.freshness_ = {15, 15, 900};
        msp2.producerParams_.segmentSize_ = 1000;

        GeneralConsumerParams gcpa;

        gcpa.interestLifetime_ = 2000;
        gcpa.jitterSizeMs_ = 150;

        GeneralConsumerParams gcpv;

        gcpv.interestLifetime_ = 2000;
        gcpv.jitterSizeMs_ = 150;

        StatGatheringParams sgp("buffer");
        static const char *s[] = {"jitterPlay", "jitterTar", "drdPrime"};
        vector<string> v(s, s + 3);
        sgp.addStats(v);

        ConsumerClientParams ccp;
        ccp.generalAudioParams_ = gcpa;
        ccp.generalVideoParams_ = gcpv;
        ccp.statGatheringParams_.push_back(sgp);

        ccp.fetchedStreams_.push_back(msp1);
        ccp.fetchedStreams_.push_back(msp2);

        cp.setConsumerParams(ccp);
    }

    return cp;
}

ClientParams sampleProducerParams()
{
    ClientParams cp;

    {
        GeneralParams gp;

        gp.loggingLevel_ = ndnlog::NdnLoggerDetailLevelAll;
        gp.logFile_ = "ndnrtc.log";
        gp.logPath_ = "/tmp";
        gp.useFec_ = false;
        gp.useAvSync_ = true;
        gp.host_ = "aleph.ndn.ucla.edu";
        gp.portNum_ = 6363;
        cp.setGeneralParameters(gp);
    }

    {
        ProducerClientParams pcp;

        pcp.prefix_ = "/ndn/edu/ucla/remap/client1";

        {
            ProducerStreamParams msp;
            stringstream ss;

            msp.sessionPrefix_ = pcp.prefix_;
            msp.streamName_ = "mic";
            msp.type_ = MediaStreamParams::MediaStreamTypeAudio;
            msp.producerParams_.freshness_ = {15, 15, 900};
            msp.producerParams_.segmentSize_ = 1000;

            CaptureDeviceParams cdp;
            cdp.deviceId_ = 0;
            msp.captureDevice_ = cdp;
            msp.addMediaThread(AudioThreadParams("pcmu"));
            msp.addMediaThread(AudioThreadParams("g722"));

            pcp.publishedStreams_.push_back(msp);
        }

        {
            ProducerStreamParams msp;
            stringstream ss;

            msp.sessionPrefix_ = pcp.prefix_;
            msp.streamName_ = "camera";
            msp.source_ = {"/tmp/camera.argb", "file"};
            msp.type_ = MediaStreamParams::MediaStreamTypeVideo;
            msp.synchronizedStreamName_ = "mic";
            msp.producerParams_.freshness_ = {15, 15, 900};
            msp.producerParams_.segmentSize_ = 1000;

            CaptureDeviceParams cdp;
            cdp.deviceId_ = 11;
            msp.captureDevice_ = cdp;
            msp.addMediaThread(VideoThreadParams("low", sampleVideoCoderParams()));
            msp.addMediaThread(VideoThreadParams("hi", sampleVideoCoderParams()));

            pcp.publishedStreams_.push_back(msp);
        }

        cp.setProducerParams(pcp);
    }

    return cp;
}

WebRtcVideoFrame getFrame(int w, int h, bool randomNoise)
{
    int width = w, height = h;
    int frameSize = width * height * 4 * sizeof(uint8_t);
    uint8_t *frameBuffer = (uint8_t *)malloc(frameSize);
    // make gardient frame (black to white vertically)
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i)
            if (randomNoise)
                frameBuffer[i * width + j] = std::rand() % 256; // random noise
            else
                frameBuffer[i * width + j] = (i % 4 ? (uint8_t)(255 * ((double)j / (double)height)) : 255);

    WebRtcSmartPtr<WebRtcVideoFrameBuffer> videoBuffer;
    {
        // make conversion to I420
        const webrtc::VideoType commonVideoType = RawVideoTypeToCommonVideoVideoType(webrtc::kVideoARGB);
        int stride_y = width;
        int stride_uv = (width + 1) / 2;
        int target_width = width;
        int target_height = height;

        videoBuffer = webrtc::I420Buffer::Create(width, height, stride_y, stride_uv, stride_uv);

        // convertedFrame.CreateEmptyFrame(target_width,
        //  abs(target_height), stride_y, stride_uv, stride_uv);
        ConvertToI420(commonVideoType, frameBuffer, 0, 0, // No cropping
                      width, height, frameSize, webrtc::kVideoRotation_0, videoBuffer.get());
    }
    free(frameBuffer);

    return WebRtcVideoFrame(videoBuffer, webrtc::kVideoRotation_0, 0);
}

std::vector<WebRtcVideoFrame> getFrameSequence(int w, int h, int len)
{
    std::srand(std::time(0));
    std::vector<WebRtcVideoFrame> frames;
    for (int i = 0; i < len; ++i)
        frames.push_back(std::move(getFrame(w, h, true)));
    return frames;
}

webrtc::EncodedImage encodedImage(size_t frameLen, uint8_t *&buffer, bool delta)
{
    int32_t size = webrtc::CalcBufferSize(webrtc::kI420, 640, 480);
    buffer = (uint8_t *)malloc(frameLen);
    for (int i = 0; i < frameLen; ++i)
        buffer[i] = i % 255;

    webrtc::EncodedImage frame(buffer, frameLen, size);
    frame._encodedWidth = 640;
    frame._encodedHeight = 480;
    frame._timeStamp = 1460488589;
    frame.capture_time_ms_ = 1460488569;
    frame._frameType = (delta ? webrtc::kVideoFrameDelta : webrtc::kVideoFrameKey);
    frame._completeFrame = true;

    return frame;
}

bool checkVideoFrame(const webrtc::EncodedImage &image)
{
    bool identical = true;
    uint8_t *buf;
    webrtc::EncodedImage ref = encodedImage(image._length, buf, (image._frameType == webrtc::kVideoFrameDelta));

    identical = (image._encodedWidth == ref._encodedWidth) &&
                (image._encodedHeight == ref._encodedHeight) &&
                (image._timeStamp == ref._timeStamp) &&
                (image.capture_time_ms_ == ref.capture_time_ms_) &&
                (image._frameType == ref._frameType) &&
                (image._completeFrame == ref._completeFrame) &&
                (image._length == ref._length);

    for (int i = 0; i < image._length && identical; ++i)
        identical &= (image._buffer[i] == ref._buffer[i]);

    free(buf);
    return identical;
}

VideoFramePacket getVideoFramePacket(size_t frameLen, double rate, int64_t pubTs,
                                     int64_t pubUts)
{
    CommonHeader hdr;
    hdr.sampleRate_ = rate;
    hdr.publishTimestampMs_ = pubTs;
    hdr.publishUnixTimestamp_ = pubUts;

    uint8_t *buffer;

    webrtc::EncodedImage frame = encodedImage(frameLen, buffer);

    VideoFramePacket vp(frame);
    std::map<std::string, PacketNumber> syncList = boost::assign::map_list_of("hi", 341)("mid", 433)("low", 432);

    vp.setSyncList(syncList);
    vp.setHeader(hdr);

    free(buffer);

    return vp;
}

std::vector<VideoFrameSegment> sliceFrame(VideoFramePacket &vp,
                                          PacketNumber playNo, PacketNumber pairedSeqNo)
{
    boost::shared_ptr<NetworkData> parity = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);

    std::vector<VideoFrameSegment> frameSegments = VideoFrameSegment::slice(vp, 1000);
    std::vector<CommonSegment> paritySegments = CommonSegment::slice(*parity, 1000);

    // pack segments into data objects
    int idx = 0;
    VideoFrameSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;
    header.totalSegmentsNum_ = frameSegments.size();
    header.playbackNo_ = playNo;
    header.pairedSequenceNo_ = pairedSeqNo;
    header.paritySegmentsNum_ = paritySegments.size();

    for (auto &s : frameSegments)
    {
        VideoFrameSegmentHeader hdr = header;
        hdr.interestNonce_ += idx;
        hdr.interestArrivalMs_ += idx;
        hdr.playbackNo_ += idx;
        idx++;
        s.setHeader(hdr);
    }

    return frameSegments;
}

std::vector<VideoFrameSegment> sliceParity(VideoFramePacket &vp, boost::shared_ptr<NetworkData> &parity)
{
    DataSegmentHeader header;
    header.interestNonce_ = 0x1234;
    header.interestArrivalMs_ = 1460399362;
    header.generationDelayMs_ = 200;

    parity = vp.getParityData(VideoFrameSegment::payloadLength(1000), 0.2);
    assert(parity.get());

    std::vector<VideoFrameSegment> paritySegments = VideoFrameSegment::slice(*parity, 1000);

    int idx = 0;
    for (auto &s : paritySegments)
    {
        DataSegmentHeader hdr = header;
        hdr.interestNonce_ += idx;
        s.setHeader(hdr);
    }

    return paritySegments;
}

std::vector<boost::shared_ptr<ndn::Data>>
dataFromSegments(std::string frameName,
                 const std::vector<VideoFrameSegment> &segments)
{
    std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
    int segIdx = 0;
    for (auto &s : segments)
    {
        ndn::Name n(frameName);
        n.appendSegment(segIdx++);
        boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
        ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size() - 1));
        ds->setContent(s.getNetworkData()->getData(), s.size());
        dataSegments.push_back(ds);
    }

    return dataSegments;
}

std::vector<boost::shared_ptr<ndn::Data>>
dataFromParitySegments(std::string frameName,
                       const std::vector<VideoFrameSegment> &segments)
{
    std::vector<boost::shared_ptr<ndn::Data>> dataSegments;
    int segIdx = 0;
    for (auto &s : segments)
    {
        ndn::Name n(frameName);
        n.append(NameComponents::NameComponentParity).appendSegment(segIdx++);
        boost::shared_ptr<ndn::Data> ds(boost::make_shared<ndn::Data>(n));
        ds->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(segments.size() - 1));
        ds->setContent(s.getNetworkData()->getData(), s.size());
        dataSegments.push_back(ds);
    }

    return dataSegments;
}

std::vector<boost::shared_ptr<Interest>>
getInterests(std::string frameName, unsigned int startSeg, size_t nSeg,
             unsigned int parityStartSeg, size_t parityNSeg, unsigned int startNonce)
{
    std::vector<boost::shared_ptr<Interest>> interests;

    int nonce = startNonce;
    for (int i = startSeg; i < startSeg + nSeg; ++i)
    {
        Name n(frameName);
        n.appendSegment(i);
        interests.push_back(boost::make_shared<Interest>(n, 1000));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        interests.back()->setNonce(Blob((uint8_t *)&(nonce), sizeof(int)));
#pragma GCC diagnostic pop
        nonce++;
    }

    nonce = startNonce;
    for (int i = parityStartSeg; i < parityStartSeg + parityNSeg; ++i)
    {
        Name n(frameName);
        n.append(NameComponents::NameComponentParity).appendSegment(i);
        interests.push_back(boost::make_shared<Interest>(n, 1000));
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        interests.back()->setNonce(Blob((uint8_t *)&(nonce), sizeof(int)));
#pragma GCC diagnostic pop
        nonce++;
    }

    return interests;
}

std::vector<boost::shared_ptr<const Interest>>
makeInterestsConst(const std::vector<boost::shared_ptr<Interest>> &interests)
{
    std::vector<boost::shared_ptr<const Interest>> cinterests;
    for (auto &i : interests)
        cinterests.push_back(boost::make_shared<const Interest>(*i));
    return cinterests;
}

boost::shared_ptr<WireSegment>
getFakeSegment(std::string threadPrefix, SampleClass cls, SegmentClass segCls,
               PacketNumber pNo, unsigned int segNo)
{
    Name n(threadPrefix);
    n.append((cls == SampleClass::Delta ? NameComponents::NameComponentDelta : NameComponents::NameComponentKey))
        .appendSequenceNumber(pNo);

    if (segCls == SegmentClass::Parity)
        n.append(NameComponents::NameComponentParity);

    n.appendSegment(segNo);

    boost::shared_ptr<const Interest> interest(boost::make_shared<Interest>(n, 1000));
    boost::shared_ptr<Data> data(boost::make_shared<Data>(n));

    int nSegments = 0;

    if (cls == SampleClass::Delta)
    {
        if (segCls == SegmentClass::Parity)
            nSegments = 2;
        else
            nSegments = 10;
    }
    else
    {
        if (segCls == SegmentClass::Parity)
            nSegments = 6;
        else
            nSegments = 30;
    }

    data->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(nSegments - 1));

    // add phony data
    VideoFramePacket vfp = getVideoFramePacket();
    std::vector<VideoFrameSegment> segments = sliceFrame(vfp);
    if (segNo == 0)
        data->setContent(segments[0].getNetworkData()->getData(), segments[0].size());
    else
        data->setContent(segments[1].getNetworkData()->getData(), segments[1].size());

    return boost::make_shared<WireData<VideoFrameSegmentHeader>>(data, interest);;
}

boost::shared_ptr<ndnrtc::WireSegment>
getFakeThreadMetadataSegment(std::string threadPrefix, const ndnrtc::DataPacket &metadata)
{
    boost::shared_ptr<WireSegment> segment;

    Name n(threadPrefix);
    n.append(NameComponents::NameComponentMeta)
        .appendVersion(0)
        .appendSegment(0);

    boost::shared_ptr<const Interest> interest(boost::make_shared<Interest>(n, 1000));
    boost::shared_ptr<Data> data(boost::make_shared<Data>(n));

    data->getMetaInfo().setFinalBlockId(ndn::Name::Component::fromSegment(0));

    std::vector<CommonSegment> segs = CommonSegment::slice(metadata, 10000);
    DataSegmentHeader hdr;

    segs[0].setHeader(hdr);
    data->setContent(segs[0].getNetworkData()->getData(), segs[0].size());

    segment = boost::make_shared<WireSegment>(data, interest);

    return segment;
}

Name keyName(std::string s)
{
    return Name(s + "/DSK-123");
}

Name certName(Name keyName)
{
    return keyName.getSubName(0, keyName.size() - 1).append("KEY").append(keyName[-1]).append("ID-CERT").append("0");
}

boost::shared_ptr<KeyChain> memoryKeyChain(const std::string name)
{
    boost::shared_ptr<MemoryIdentityStorage> identityStorage(new MemoryIdentityStorage());
    boost::shared_ptr<MemoryPrivateKeyStorage> privateKeyStorage(new MemoryPrivateKeyStorage());
    boost::shared_ptr<KeyChain> keyChain(boost::make_shared<KeyChain>(boost::make_shared<IdentityManager>(identityStorage, privateKeyStorage),
                                                                      boost::make_shared<SelfVerifyPolicyManager>(identityStorage.get())));

    // Initialize the storage.
    Name kn = keyName(name);
    Name certificateName = certName(kn);

    identityStorage->addKey(kn, KEY_TYPE_RSA, Blob(DEFAULT_RSA_PUBLIC_KEY_DER, sizeof(DEFAULT_RSA_PUBLIC_KEY_DER)));
    privateKeyStorage->setKeyPairForKeyName(kn, KEY_TYPE_RSA, DEFAULT_RSA_PUBLIC_KEY_DER,
                                            sizeof(DEFAULT_RSA_PUBLIC_KEY_DER), DEFAULT_RSA_PRIVATE_KEY_DER,
                                            sizeof(DEFAULT_RSA_PRIVATE_KEY_DER));

    return keyChain;
}

bool checkNfd()
{
    try
    {
        boost::mutex m;
        boost::unique_lock<boost::mutex> lock(m);
        boost::condition_variable isDone;

        boost::asio::io_service io;
        boost::shared_ptr<boost::asio::io_service::work> work(boost::make_shared<boost::asio::io_service::work>(io));
        boost::thread t([&isDone, &io]() {
            try
            {
                io.run();
            }
            catch (std::exception &e)
            {
                isDone.notify_one();
            }
        });

        ThreadsafeFace f(io);

        boost::shared_ptr<KeyChain> keyChain = memoryKeyChain("/test");
        f.setCommandSigningInfo(*keyChain, certName(keyName("/test")));

        OnInterestCallback dummy = [](const ptr_lib::shared_ptr<const Name> &prefix,
                                      const ptr_lib::shared_ptr<const Interest> &interest, Face &face,
                                      uint64_t interestFilterId,
                                      const ptr_lib::shared_ptr<const InterestFilter> &filter) {};

        bool registered = false;
        f.registerPrefix(Name("/test"),
                         dummy,
                         [&isDone, &registered](const ptr_lib::shared_ptr<const Name> &) {
                             isDone.notify_one();
                         },
                         [&isDone, &registered](const ptr_lib::shared_ptr<const Name> &,
                                                uint64_t registeredPrefixId) {
                             isDone.notify_one();
                             registered = true;
                         });

        isDone.wait(lock);

        work.reset();
        io.stop();
        t.join();

        if (!registered)
            throw std::runtime_error("");
    }
    catch (std::exception &e)
    {
        GT_PRINTF("Error creating Face. NFD may not be running. Skipping this test.\n");
        return false;
    }

    return true;
}

//******************************************************************************
void DelayQueue::push(QueueBlock block)
{
    int d = (dev_ ? std::rand() % (int)(2 * dev_) - dev_ : 0);
    int fireDelayMs = delayMs_ + d;
    if (fireDelayMs < 0)
        fireDelayMs = 0;

    TPoint fireTime = Clock::now() + Msec(fireDelayMs);

    {
        boost::lock_guard<boost::recursive_mutex> scopedLock(m_);
        queue_[fireTime].push_back(block);
    }

    if (!timerSet_)
    {
        timerSet_ = true;
        timer_.expires_at(fireTime);
        timer_.async_wait(boost::bind(&DelayQueue::pop, this,
                                      boost::asio::placeholders::error));
    }
}

void DelayQueue::reset()
{
    timer_.cancel();
    timerSet_ = false;
}

void DelayQueue::pop(const boost::system::error_code &e)
{
    if (e != boost::asio::error::operation_aborted && isActive())
    {
        std::vector<QueueBlock> first;
        {
            boost::lock_guard<boost::recursive_mutex> scopedLock(m_);
            first = queue_.begin()->second;

            if (queue_.size() > 1)
            {
                queue_.erase(queue_.begin());
                timer_.expires_at(queue_.begin()->first);
                timer_.async_wait(boost::bind(&DelayQueue::pop, this,
                                              boost::asio::placeholders::error));
            }
            else
            {
                queue_.erase(queue_.begin());
                timerSet_ = false;
            }
        }

        for (auto &block : first)
            block();
    }
}

size_t DelayQueue::size() const
{
    boost::lock_guard<boost::recursive_mutex> scopedLock(m_);
    return queue_.size();
}

void DataCache::addInterest(const boost::shared_ptr<ndn::Interest> interest, OnDataT onData)
{

    if (data_.find(interest->getName()) != data_.end())
    {
        boost::shared_ptr<ndn::Data> d;
        OnInterestT onInterest;
        {
            boost::lock_guard<boost::mutex> scopedLock(m_);
            d = data_[interest->getName()];

            if (onInterestCallbacks_.find(interest->getName()) != onInterestCallbacks_.end())
            {
                onInterest = onInterestCallbacks_[interest->getName()];
                onInterestCallbacks_.erase(onInterestCallbacks_.find(interest->getName()));
            }

            data_.erase(data_.find(interest->getName()));
        }

        if (onInterest)
            onInterest(interest);
        onData(d, interest);
    }
    else
    {
        boost::lock_guard<boost::mutex> scopedLock(m_);

        interests_[interest->getName()] = interest;
        onDataCallbacks_[interest->getName()] = onData;
    }
}

void DataCache::addData(const boost::shared_ptr<ndn::Data> &data, OnInterestT onInterest)
{
    if (interests_.find(data->getName()) != interests_.end())
    {
        OnDataT onData = onDataCallbacks_[data->getName()];
        boost::shared_ptr<ndn::Interest> i;
        {
            boost::lock_guard<boost::mutex> scopedLock(m_);
            i = interests_[data->getName()];
            interests_.erase(interests_.find(data->getName()));
            onDataCallbacks_.erase(onDataCallbacks_.find(data->getName()));
        }

        if (onInterest)
            onInterest(i);
        onData(data, i);
    }
    else
    {
        boost::lock_guard<boost::mutex> scopedLock(m_);

        data_[data->getName()] = data;
    }
}
