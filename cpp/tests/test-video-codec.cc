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

TEST(TestCodec, TestImageCopy)
{
    {
        size_t w = 1280, h = 720;
        size_t i420_size = 3*w*h/2;
        FILE *fIn = fopen((resources_path+"/eb_samples/eb_dog_1280x720_240.yuv").c_str(), "rb");
        if (!fIn)
            FAIL() << "couldn't open input video file";

        uint8_t *rawData = (uint8_t*)malloc(i420_size);
        int n = fread(rawData, 1, i420_size, fIn);
        ASSERT_TRUE(n == i420_size);

        VideoCodec::Image raw(w, h, ImageFormat::I420, rawData);
        EXPECT_EQ(raw.getWidth(), w);
        EXPECT_EQ(raw.getHeight(), h);

        uint8_t *copiedData = (uint8_t*)malloc(i420_size);
        memset(copiedData, 0, i420_size);

        raw.copyTo(copiedData);

        EXPECT_EQ(memcmp(rawData, copiedData, i420_size), 0);
   }
   {
       size_t w = 1280, h = 720;
       size_t i420_size = 3*w*h/2;
       FILE *fIn = fopen((resources_path+"/eb_samples/eb_dog_1280x720_240.yuv").c_str(), "rb");
       if (!fIn)
           FAIL() << "couldn't open input video file";

       uint8_t *rawData = (uint8_t*)malloc(i420_size);
       int n = fread(rawData, 1, i420_size, fIn);
       ASSERT_TRUE(n == i420_size);

       vpx_image_t *vpx_img = nullptr;
       vpx_img = vpx_img_wrap(vpx_img, VPX_IMG_FMT_I420, w, h, 0, rawData);
       ASSERT_TRUE(vpx_img != nullptr);

       VideoCodec::Image raw(vpx_img);
       EXPECT_EQ(raw.getDataSize(), i420_size);
       EXPECT_EQ(raw.getWidth(), w);
       EXPECT_EQ(raw.getHeight(), h);

       uint8_t *copiedData = (uint8_t*)malloc(i420_size);
       memset(copiedData, 0, i420_size);

       raw.copyTo(copiedData);

       EXPECT_EQ(memcmp(rawData, copiedData, i420_size), 0);
   }
}

TEST(TestCodec, TestCreate)
{
    // { // uninitialized settings
    //     CodecSettings s;
    //     VideoCodec vc;
    //
    //     EXPECT_ANY_THROW(vc.initEncoder(s));
    // }
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

    CodecSettings s = VideoCodec::defaultCodecSettings();
    s.spec_.decoder_.frameBufferList_ = nullptr;
    dc.initDecoder(s);

    int nRead = 0, nBytes = 0, nDropped = 0, nEncoded = 0, nKey = 0, nDecoded = 0;
    while (raw.read(fIn))
    {
        EXPECT_EQ(raw.getWidth(), 1280);
        EXPECT_EQ(raw.getHeight(), 720);
        nRead ++;
        int res = ec.encode(raw, false,
            [&nEncoded, &nDecoded, &nBytes, &nKey, &dc](const EncodedFrame& frame){
                nEncoded++;
                nBytes += frame.length_;
                if (frame.type_ == FrameType::Key)
                    nKey++;

                int res = dc.decode(frame,
                                    [&nDecoded](const VideoCodec::Image& raw){
                                        EXPECT_EQ(raw.getWidth(), 1280);
                                        EXPECT_EQ(raw.getHeight(), 720);
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

class FrameBufferList : public ndnrtc::IFrameBufferList {
    typedef struct _FrameBuffer {
        uint8_t *data_;
        size_t dataSize_;
        bool inUse_;
    } FrameBuffer;

public:
    ~FrameBufferList(){
        for (int i = 0; i < nBuffers_; ++i)
            free(fbList_[i].data_);
        free(fbList_);
    }

    virtual int getFreeFrameBuffer(size_t minSize, vpx_codec_frame_buffer_t *fb)
    {
        nGetCalls_++;
        int bufIdx = getFreeBuffer();

        if (fbList_[bufIdx].dataSize_ < minSize)
        {
            free(fbList_[bufIdx].data_);
            fbList_[bufIdx].data_ = (uint8_t*)malloc(minSize);
            memset(fbList_[bufIdx].data_, 0, minSize);
            fbList_[bufIdx].dataSize_ = minSize;
        }

        fb->data = fbList_[bufIdx].data_;
        fb->size = fbList_[bufIdx].dataSize_;
        fb->priv = (void*)bufIdx;
        fbList_[bufIdx].inUse_ = true;

        return 0;
    }

    virtual int returnFrameBuffer(vpx_codec_frame_buffer_t *fb)
    {
        nReturnCalls_++;
        int64_t bufIdx = reinterpret_cast<int64_t>(fb->priv);
        assert(bufIdx < nBuffers_);
        FrameBuffer &extFb = fbList_[bufIdx];
        assert(extFb.inUse_);
        extFb.inUse_ = false;

        return 0;
    }

    virtual size_t getUsedBufferNum()
    {
        size_t nFree = 0;
        for (int i = 0; i < nBuffers_; ++i)
            if (!fbList_[i].inUse_)
                nFree++;
        return nFree;
    }

    virtual size_t getFreeBufferNum()
    {
        size_t nFree = 0;
        for (int i = 0; i < nBuffers_; ++i)
            if (!fbList_[i].inUse_)
                nFree++;
        return nFree;
    }

    int nGetCalls_ = 0;
    int nReturnCalls_ = 0;
private:

    size_t nBuffers_ = 0;
    FrameBuffer *fbList_;

    int getFreeBuffer() {
        if (!getFreeBufferNum())
        {
            int firstFreeBufIdx = nBuffers_;
            if (nBuffers_ == 0)
            {
                nBuffers_ = 10;
                fbList_ = (FrameBuffer*)malloc(sizeof(FrameBuffer)*nBuffers_);
            }
            else
            {
                nBuffers_ *= 2;
                // allocate more
                fbList_ = (FrameBuffer*)realloc(fbList_, sizeof(FrameBuffer)*nBuffers_);
            }

            for (int i = firstFreeBufIdx; i < nBuffers_; ++i)
                memset(&fbList_[i], 0, sizeof(FrameBuffer));
            return firstFreeBufIdx;
        }
        else
        {
            int idx = getFirstFreeBuffer();
            assert(idx >= 0);
            return idx;
        }
    }

    int getFirstFreeBuffer(){
        for (int i = 0; i < nBuffers_; ++i)
            if (!fbList_[i].inUse_)
                return i;
        return -1;
    }
};

static int getFrameBuffer(void *user_priv, size_t min_size, vpx_codec_frame_buffer_t *fb)
{
    IFrameBufferList *fbList = reinterpret_cast<IFrameBufferList*>(user_priv);
    return fbList->getFreeFrameBuffer(min_size, fb);
}

static int releaseFrameBuffer(void *user_priv, vpx_codec_frame_buffer_t *fb)
{
    IFrameBufferList *fbList = reinterpret_cast<IFrameBufferList*>(user_priv);
    return fbList->returnFrameBuffer(fb);
}

TEST(TestCodec, TestEncodeDecodeExtFrameBuffer)
{
    FILE *fIn = fopen((resources_path+"/eb_samples/eb_dog_1280x720_240.yuv").c_str(), "rb");
    if (!fIn)
        FAIL() << "couldn't open input video file";
    FrameBufferList fbList;
    int nRead = 0, nBytes = 0, nDropped = 0, nEncoded = 0, nKey = 0, nDecoded = 0;
    {
    VideoCodec ec, dc;
    ec.initEncoder(VideoCodec::defaultCodecSettings());
    VideoCodec::Image raw(ec.getSettings().spec_.encoder_.width_,
                          ec.getSettings().spec_.encoder_.height_,
                          ImageFormat::I420);

    CodecSettings s = VideoCodec::defaultCodecSettings();
    s.spec_.decoder_.frameBufferList_ = &fbList;
    dc.initDecoder(s);

    while (raw.read(fIn))
    {
        EXPECT_EQ(raw.getWidth(), 1280);
        EXPECT_EQ(raw.getHeight(), 720);
        nRead ++;
        int res = ec.encode(raw, false,
            [&nEncoded, &nDecoded, &nBytes, &nKey, &dc](const EncodedFrame& frame){
                nEncoded++;
                nBytes += frame.length_;
                if (frame.type_ == FrameType::Key)
                    nKey++;

                int res = dc.decode(frame,
                                    [&nDecoded](const VideoCodec::Image& raw){
                                        EXPECT_EQ(raw.getWidth(), 1280);
                                        EXPECT_EQ(raw.getHeight(), 720);
                                        nDecoded++;
                                    });
                EXPECT_EQ(res, 0);
            },
            [&nDropped](const VideoCodec::Image&){
                    nDropped++;
                });
        EXPECT_EQ(res, 0);
    }
    }

    EXPECT_EQ(fbList.nGetCalls_, 240);
    // TODO: figure out what this should be
    // EXPECT_EQ(fbList.nReturnCalls_, 240-nDropped);
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
        if (resPath.leaf() == ".libs")
            resPath /= boost::filesystem::path("..");

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
