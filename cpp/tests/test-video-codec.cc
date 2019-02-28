//
// test-video-codec.cc
//
//  Created by Peter Gusev on 25 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include <cstdlib>
#include <stdlib.h>
#include <iostream>

#include <boost/filesystem.hpp>

#include "gtest/gtest.h"
#include "include/ndnrtc-common.hpp"
#include "src/video-codec.hpp"

using namespace ::testing;
using namespace ndnrtc;
using namespace std;

string resources_path = "";
string test_path = "";

TEST(TestCodec, TestImage)
{
    {
        size_t w = 1280, h = 720;
        size_t i420_size = 3*w*h/2;
        VideoCodec::Image raw(w, h, ImageFormat::I420);
        EXPECT_EQ(raw.getWidth(), w);
        EXPECT_EQ(raw.getHeight(), h);
        EXPECT_EQ(raw.getFormat(), ImageFormat::I420);

        EXPECT_EQ(raw.getAllocationSize(), i420_size);
        EXPECT_EQ(raw.getAllocatedSize(), i420_size);
        EXPECT_EQ(nullptr, raw.getUserData());
        EXPECT_FALSE(raw.getIsWrapped());

        FILE *fIn = fopen((resources_path+"/eb_samples/eb_dog_1280x720_240.yuv").c_str(), "rb");
        if (!fIn)
            FAIL() << "couldn't open input video file";

        int res = 0;
        EXPECT_NO_THROW(res = raw.read(fIn));
        EXPECT_NE(res, 0);

        vpx_image_t *vpxImage = nullptr;
        raw.toVpxImage(&vpxImage);

        EXPECT_EQ(w, vpxImage->d_w);
        EXPECT_EQ(h, vpxImage->d_h);
   }
}

TEST(TestCodec, TestCreate)
{
    { // uninitialized settings
        CodecSettings s;
        VideoCodec vc;

        EXPECT_ANY_THROW(vc.initEncoder(s));
    }
    { // default settings
        VideoCodec vc;
        EXPECT_NO_THROW(vc.initEncoder(VideoCodec::defaultCodecSettings()));
    }
    { // default settings
        VideoCodec vc;
        EXPECT_NO_THROW(vc.initDecoder(VideoCodec::defaultCodecSettings()));
    }
    { // double init
        CodecSettings s;
        s.numCores_ = 8;
        s.rowMt_ = 1;
        s.spec_.encoder_.width_ = 1280;
        s.spec_.encoder_.height_ = 720;
        s.spec_.encoder_.bitrate_ = 8000;
        s.spec_.encoder_.gop_ = 30;
        s.spec_.encoder_.fps_ = 30;
        s.spec_.encoder_.dropFrames_ = false;
        VideoCodec vc;

        EXPECT_NO_THROW(vc.initEncoder(s));
        EXPECT_ANY_THROW(vc.initDecoder(s));
    }
    { // double init
        CodecSettings s;
        s.numCores_ = 8;
        s.rowMt_ = 1;
        VideoCodec vc;

        EXPECT_NO_THROW(vc.initDecoder(s));
        EXPECT_ANY_THROW(vc.initEncoder(s));
    }
}

TEST(TestCodec, TestEncodeDecode)
{
    FILE *fIn = fopen((resources_path+"/eb_samples/eb_dog_1280x720_240.yuv").c_str(), "rb");
    if (!fIn)
        FAIL() << "couldn't open input video file";

    VideoCodec ec, dc;
    ec.initEncoder(VideoCodec::defaultCodecSettings());
    VideoCodec::Image raw(ec.getSettings().spec_.encoder_.width_,
                          ec.getSettings().spec_.encoder_.height_,
                          ImageFormat::I420);

    dc.initDecoder(VideoCodec::defaultCodecSettings());

    int nRead = 0, nBytes = 0, nDropped = 0, nEncoded = 0, nKey = 0, nDecoded = 0;
    while (raw.read(fIn))
    {
        nRead ++;
        int res = ec.encode(raw, false,
            [&nEncoded, &nDecoded, &nBytes, &nKey, &dc](const EncodedFrame& frame){
                nEncoded++;
                nBytes += frame.length_;
                if (frame.type_ == FrameType::Key)
                    nKey++;

                int res = dc.decode(frame,
                                    [&nDecoded](const VideoCodec::Image& raw){
                                        nDecoded++;
                                    });
                EXPECT_EQ(res, 0);
            },
            [&nDropped](const VideoCodec::Image&){
                    nDropped++;
                });
        EXPECT_EQ(res, 0);
    }

    EXPECT_EQ(nRead, 240);
    EXPECT_EQ(nDropped+nEncoded, 240);
    EXPECT_EQ(nEncoded, nDecoded);
    EXPECT_EQ(nKey, 8);
    fclose(fIn);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    #ifdef HAVE_BOOST_FILESYSTEM
        boost::filesystem::path testPath(boost::filesystem::system_complete(argv[0]).remove_filename());
        test_path = testPath.string();

        boost::filesystem::path resPath(testPath);
        #ifndef __ANDROID__
        resPath /= boost::filesystem::path("..") /=
                   boost::filesystem::path("..") /=
                   boost::filesystem::path("resources");
        #else
        resPath = test_path;
        #endif

        resources_path = resPath.string();
    #else
        test_path = std::string(argv[0]);
        std::vector<std::string> comps;
        boost::split(comps, test_path, boost::is_any_of("/"));

        test_path = "";
        for (int i = 0; i < comps.size()-1; ++i)
        {
            test_path += comps[i];
            if (i != comps.size()-1) test_path += "/";
        }

        #ifndef __ANDROID__
        resources_path = test_path + "../../resources";
        #else
        resources_path = test_path;
        #endif
    #endif

    return RUN_ALL_TESTS();
}
