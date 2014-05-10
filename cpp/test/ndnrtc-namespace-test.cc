//
//  ndnrtc-namespace-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 9/19/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "ndnrtc-namespace.h"

#define NDN_COMMON_HUB "ndn/edu/ucla"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(NdnRtcNamespace, TestBuildPathComponents)
{
    std::string producerId = "producer1";
    std::string hub = NDN_COMMON_HUB;
    std::string stream = "video0";
    
    {
        shared_ptr<std::string> path = NdnRtcNamespace::buildPath(false, NULL);
        EXPECT_EQ("",*(path.get()));
    }
    {
        shared_ptr<std::string> path = NdnRtcNamespace::buildPath(true, NULL);
        EXPECT_EQ("",*(path.get()));
    }
    {
        shared_ptr<std::string> path = NdnRtcNamespace::buildPath(true, &hub, &producerId, &stream, NULL);
        char str[256];
        
        memset(str, 0, 256);
        sprintf(str, "/%s/%s/%s",hub.c_str(), producerId.c_str(), stream.c_str());
        
        EXPECT_EQ(str,*(path.get()));
    }
    {
        shared_ptr<std::string> path = NdnRtcNamespace::buildPath(false, &hub, &producerId, &stream, NULL);
        char str[256];
        
        memset(str, 0, 256);
        sprintf(str, "%s/%s/%s",hub.c_str(), producerId.c_str(), stream.c_str());
        
        EXPECT_EQ(str,*(path.get()));
    }
    
}

TEST(NdnRtcNamespace, TestUserPrefix)
{
    std::string producerId = "producer1";
    std::string hub = NDN_COMMON_HUB;
    std::string stream = "video0";
    
    {
        shared_ptr<std::string> path = NdnRtcNamespace::getProducerPrefix(hub, producerId);
        char str[256];
        
        memset(str, 0, 256);
        sprintf(str, "/%s/%s/%s/%s",
                hub.c_str(),
                NdnRtcNamespace::NameComponentApp.c_str(),
                NdnRtcNamespace::NameComponentUser.c_str(),
                producerId.c_str());
        
        EXPECT_EQ(str,*(path.get()));
    }
}

TEST(NdnRtcNamespace, TestStreamPath)
{
    std::string producerId = "producer1";
    std::string hub = NDN_COMMON_HUB;
    std::string stream = "video0";
    
    {
        shared_ptr<std::string> path = NdnRtcNamespace::getStreamPath(hub, producerId, stream);
        char str[256];
        
        memset(str, 0, 256);
        sprintf(str, "/%s/%s/%s/%s/%s/%s",hub.c_str(),
                NdnRtcNamespace::NameComponentApp.c_str(),
                NdnRtcNamespace::NameComponentUser.c_str(),
                producerId.c_str(),
                NdnRtcNamespace::NameComponentUserStreams.c_str(),
                stream.c_str());
        
        EXPECT_EQ(str,*(path.get()));
    }
}

TEST(NdnRtcNamespace, TestKeyPath)
{
    std::string producerId = "producer1";
    std::string hub = NDN_COMMON_HUB;
    std::string stream = "video0";
    
    {
        shared_ptr<std::string> producerPref = NdnRtcNamespace::getProducerPrefix(hub, producerId);
        shared_ptr<Name> keyPrefix = NdnRtcNamespace::keyPrefixForUser(*producerPref);
        char str[256];
        
        memset(str, 0, 256);
        
        sprintf(str, "/%s/%s/%s/%s/%s/%s",hub.c_str(),
                NdnRtcNamespace::NameComponentApp.c_str(),
                NdnRtcNamespace::NameComponentUser.c_str(),
                producerId.c_str(),
                NdnRtcNamespace::NameComponentStreamKey.c_str(),
                NdnRtcNamespace::KeyComponent.c_str());
        EXPECT_STREQ(str,keyPrefix->toUri().c_str());
    }
}

TEST(NdnRtcNamespace, TestCertPath)
{
    std::string producerId = "producer1";
    std::string hub = NDN_COMMON_HUB;
    std::string stream = "video0";
    
    {
        shared_ptr<Name> keyPrefix = NdnRtcNamespace::certificateNameForUser(*NdnRtcNamespace::getProducerPrefix(hub, producerId));
        char str[256];
        
        memset(str, 0, 256);
        
        sprintf(str, "/%s/%s/%s/%s/%s/%s/%s",hub.c_str(),
                NdnRtcNamespace::NameComponentApp.c_str(),
                NdnRtcNamespace::NameComponentUser.c_str(),
                producerId.c_str(),
                NdnRtcNamespace::NameComponentStreamKey.c_str(),
                NdnRtcNamespace::KeyComponent.c_str(),
                NdnRtcNamespace::CertificateComponent.c_str());
        EXPECT_STREQ(str,keyPrefix->toUri().c_str());
    }
}

TEST(NdnRtcNamespace, TestNumberComponent)
{
    {
        char *number = "10";
        long unsigned int num = atoi(number);
        
        shared_ptr<const std::vector<unsigned char>> comp = NdnRtcNamespace::getNumberComponent(num);
        
        for (int i = 0; i < strlen(number); i++)
            EXPECT_EQ(number[i], (*comp)[i]);
    }
    {
        char *number = "0";
        long unsigned int num = atoi(number);
        
        shared_ptr<const std::vector<unsigned char>> comp = NdnRtcNamespace::getNumberComponent(num);
        
        for (int i = 0; i < strlen(number); i++)
            EXPECT_EQ(number[i], (*comp)[i]);
    }
    {
        char *number = "1000000";
        long unsigned int num = atoi(number);
        
        shared_ptr<const std::vector<unsigned char>> comp = NdnRtcNamespace::getNumberComponent(num);
        
        for (int i = 0; i < strlen(number); i++)
            EXPECT_EQ(number[i], (*comp)[i]);
    }
    { // check max
        char *number = "18446744073709551615";
        long unsigned int num = (unsigned long)-1;
        
        shared_ptr<const std::vector<unsigned char>> comp = NdnRtcNamespace::getNumberComponent(num);
        
        for (int i = 0; i < strlen(number); i++)
            EXPECT_EQ(number[i], (*comp)[i]);
    }
}

TEST(NdnRtcNamespace, TestCheckComponents)
{
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta/1/%00%04");
        
        EXPECT_TRUE(NdnRtcNamespace::isDeltaFramesPrefix(prefix));
        EXPECT_FALSE(NdnRtcNamespace::isKeyFramePrefix(prefix));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "user"));
        EXPECT_FALSE(NdnRtcNamespace::hasComponent(prefix, "video"));
        
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "video0"));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "video0/"));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "/video0"));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "/video0/"));
        
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "video0/vp8"));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "video0/vp8/frames/"));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "/video0/vp8/frames/"));
        
        EXPECT_FALSE(NdnRtcNamespace::hasComponent(prefix, "streams/video"));
        EXPECT_FALSE(NdnRtcNamespace::hasComponent(prefix, "testuser/streams/video"));
        EXPECT_TRUE(NdnRtcNamespace::hasComponent(prefix, "testuser/streams"));
        EXPECT_FALSE(NdnRtcNamespace::hasComponent(prefix, "testuser//streams"));
    }
    
    {
         Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key/1/%00%04");
        
        EXPECT_TRUE(NdnRtcNamespace::isKeyFramePrefix(prefix));
        EXPECT_FALSE(NdnRtcNamespace::isDeltaFramesPrefix(prefix));
    }
}

TEST(NdnRtcNamespace, TestHelperFunctions)
{
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta/1/data/%00%04");
        
        EXPECT_EQ(1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(4, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta/1");
        
        EXPECT_EQ(1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta/1/parity/%00%04");
        prefix.appendFinalSegment(4);
        
        EXPECT_EQ(1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(4, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta");
        
        EXPECT_EQ(-1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/");
        
        EXPECT_EQ(-1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key/1/data/%00%04");
        
        EXPECT_EQ(1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(4, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key/1");
        
        EXPECT_EQ(1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key/1/parity/%00%04/%C1.M.FINAL%00%06");
        prefix.appendFinalSegment(4);
        
        EXPECT_EQ(1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(4, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key");
        
        EXPECT_EQ(-1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    
    {// no streams component
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/video0/vp8/frames/key/1/data/%00%04");
        
        EXPECT_EQ(-1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    {// no streams component
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/video0/vp8/frames/delta/1/parity/%00%04");
        
        EXPECT_EQ(-1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    
    { // no key/delta component
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/1/data/%00%04");
        
        EXPECT_EQ(-1, NdnRtcNamespace::getPacketNumber(prefix));
        EXPECT_EQ(-1, NdnRtcNamespace::getSegmentNumber(prefix));
    }
    
    { // trim segment number
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta/1/data/%00%04");
        Name trimmedPrefix;
        
        EXPECT_LE(0, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix.getComponentCount()-1, trimmedPrefix.getComponentCount());
        EXPECT_FALSE(prefix.toUri().find("%00%04") == std::string::npos);
        EXPECT_TRUE(trimmedPrefix.toUri().find("%00%04") == std::string::npos);
    }
    { // trim segment number for key
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key/1/parity/%00%04");
        Name trimmedPrefix;
        
        EXPECT_LE(0, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix.getComponentCount()-1, trimmedPrefix.getComponentCount());
        EXPECT_FALSE(prefix.toUri().find("%00%04") == std::string::npos);
        EXPECT_TRUE(trimmedPrefix.toUri().find("%00%04") == std::string::npos);
    }
    { // trim segment number for bad prefix 1
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/video0/vp8/frames/key/1/data/%00%04");
        Name trimmedPrefix;
        
        EXPECT_EQ(-1, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix, trimmedPrefix);
    }
    { // trim segment number for bad prefix 2
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/key/1/parity/%00%04");
        Name trimmedPrefix;
        
        EXPECT_EQ(-1, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix, trimmedPrefix);
    }
    { // trim segment number for bad prefix 3
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/1/data/%00%04");
        Name trimmedPrefix;
        
        EXPECT_EQ(-1, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix, trimmedPrefix);
    }
    { // trim segment number for bad prefix 4
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/delta/1");
        Name trimmedPrefix;
        
        EXPECT_EQ(-1, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix, trimmedPrefix);
    }
    { // trim segment number for bad prefix 4
        Name prefix("/ndn/edu/ucla/cs/ndnrtc/user/testuser/streams/video0/vp8/frames/key");
        Name trimmedPrefix;
        
        EXPECT_EQ(-1, NdnRtcNamespace::trimSegmentNumber(prefix, trimmedPrefix));
        EXPECT_EQ(prefix, trimmedPrefix);
    }
}


