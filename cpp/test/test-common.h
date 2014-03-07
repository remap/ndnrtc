//
//  test-common.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/29/13
//

#ifndef ndnrtc_test_common_h
#define ndnrtc_test_common_h

#define ENV_NAME std::string(__FILE__)

#include <unistd.h>
#include <stdint.h>

#include "gtest/gtest.h"
#include "simple-log.h"
#include "webrtc.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "ndnrtc-object.h"
#include "frame-buffer.h"
#include "frame-data.h"
#include "segmentizer.h"

using namespace ndnrtc;

int64_t millisecondTimestamp();

#define WAIT_(ex, timeout, res) \
do { \
res = (ex); \
int64_t start = millisecondTimestamp(); \
while (!res && millisecondTimestamp() < start + timeout) { \
usleep(1000); \
res = (ex); \
} \
} while (0);\

#define WAIT(timeout) \
do { \
bool b = false; \
WAIT_(false, timeout, b); \
} while (0); \

#define EXPECT_TRUE_WAIT(ex, timeout) \
do { \
bool res; \
WAIT_(ex, timeout, res); \
if (!res) EXPECT_TRUE(ex); \
} while (0);

class UnitTestHelper
{
public:
    static bool checkFileExists(const char *fileName)
    {
        return (access(fileName, F_OK) != -1);
    }
};

//******************************************************************************
class NdnRtcObjectTestHelper : public ::testing::Test, public ndnrtc::INdnRtcObjectObserver
{
public:
    virtual void SetUp()
    {
        flushFlags();
        
        const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
        LOG_INFO("***");
        LOG_INFO("***[GTESTS]: entering test %s:%s", test_info->test_case_name(),test_info->name());
        
#ifdef WEBRTC_LOGGING
        setupWebRTCLogging();
#endif
    }
    virtual void TearDown()
    {
        const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
        LOG_INFO("***[GTESTS]: leaving test %s:%s", test_info->test_case_name(),test_info->name());
        LOG_INFO("***");
    }
    
    virtual void onErrorOccurred(const char *errorMessage)
    {
        obtainedError_ = true;
        obtainedEmsg_ = (char*)errorMessage;
    }
    
    static webrtc::EncodedImage *loadEncodedFrame()
    {
        int width = 640, height = 480;
        // change frame size according to the data in resource file file
        
        FILE *f = fopen("resources/vp8_640x480.frame", "rb");
        
        if (!f)
        {
            return NULL;
        }
        
        int32_t size, length;
        if (!fread(&size, sizeof(size), 1, f))
        {
            return NULL;
        }
        
        if (!fread(&length, sizeof(length), 1, f))
        {
            return NULL;
        }
        
        // don't delete frameData = it is used (but not owned!) by the
        // frame when it is created
        unsigned char* frameData = new unsigned char[size];
        
        if (!fread(frameData, 1, length, f))
        {
            return NULL;
        }
        
        webrtc::EncodedImage *sampleFrame;
        sampleFrame = new webrtc::EncodedImage(frameData, length, size);
        sampleFrame->_encodedWidth = width;
        sampleFrame->_encodedHeight = height;
        sampleFrame->_timeStamp = millisecondTimestamp();
        sampleFrame->capture_time_ms_ = millisecondTimestamp();
        
        fclose(f);
        
        return sampleFrame;
    }
    
    static std::vector<shared_ptr<Data>>
    packAndSliceFrame(const webrtc::EncodedImage *frame, int frameNo, int segmentSize,
                      const Name& prefix,
                      PacketData::PacketMetadata meta,
                      PrefixMetaInfo &prefixMeta,
                      SegmentData::SegmentMetaInfo &segmentHeader)
    {
        std::vector<shared_ptr<Data>> dataSegments;
        
        // step 1 - pack into PacketData
        NdnFrameData frameData(*frame, meta);
        
        // step 2 - segmentize packet
        Segmentizer::SegmentList segments;
        Segmentizer::segmentize(frameData, segments, segmentSize);
        
        prefixMeta.totalSegmentsNum_ = segments.size();
        
        for (Segmentizer::SegmentList::iterator it = segments.begin();
             it != segments.end(); it++)
        {
            ndnrtc::new_api::FrameBuffer::Slot::Segment seg = *it;
            SegmentData segData(seg.getDataPtr(), seg.getPayloadSize(),
                                segmentHeader);
            
            Name segPrefix(prefix);
            segPrefix.append(NdnRtcUtils::componentFromInt(frameNo));
            segPrefix.appendSegment(it-segments.begin());
            segPrefix.append(PrefixMetaInfo::toName(prefixMeta));
            
            shared_ptr<Data> data(new Data(segPrefix));
            data->setContent(segData.getData(), segData.getLength());
            
            dataSegments.push_back(data);
        }
        
        return dataSegments;
    }
    
    static void checkFrames(webrtc::EncodedImage *f1, webrtc::EncodedImage *f2)
    {
        EXPECT_EQ(f1->_size, f2->_size);
        EXPECT_EQ(f1->_length, f2->_length);
        EXPECT_EQ(f1->_encodedWidth, f2->_encodedWidth);
        EXPECT_EQ(f1->_encodedHeight, f2->_encodedHeight);
        EXPECT_EQ(f1->_timeStamp, f2->_timeStamp);
        EXPECT_EQ(f1->capture_time_ms_, f2->capture_time_ms_);
        EXPECT_EQ(f1->_frameType, f2->_frameType);
        EXPECT_EQ(f1->_completeFrame, f2->_completeFrame);
        
        for (unsigned int i = 0; i < f1->_length; i++)
            EXPECT_EQ(f1->_buffer[i], f2->_buffer[i]);
    }
    
    static int randomInt(int min, int max)
    {
        return rand() % (max-min)+min;
    }
    
protected:
    char *obtainedEmsg_ = NULL;
    bool obtainedError_ = false;
    
    virtual void flushFlags()
    {
        obtainedEmsg_ = NULL;
        obtainedError_ = false;
    }
    
    void setupWebRTCLogging(){
        webrtc::Trace::CreateTrace();
        webrtc::Trace::SetTraceFile("bin/webrtc.log");
    }
};

//******************************************************************************
class NdnRtcTestEnvironment : public ::testing::Environment
{
public:
    NdnRtcTestEnvironment(const std::string &name,
                          NdnLoggerDetailLevel logLevel = NdnLoggerDetailLevelAll):Environment()
    {
        name_ = name;
        name_ += ".log";
        logLevel_ = logLevel;
    }
    void SetUp()
    {
        NdnLogger::initialize(name_.c_str(), logLevel_);
        LOG_INFO("test suite started. log is here: %s", name_.c_str());
    }
    void TearDown()
    {
        LOG_INFO("test suite finished");
    }
    
    void setLogLevel(NdnLoggerDetailLevel lvl)
    {
        logLevel_ = lvl;
    }
    
protected:
    std::string name_;
    NdnLoggerDetailLevel logLevel_;
};


//******************************************************************************
class CocoaTestEnvironment : public ::testing::Environment
{
public:
    void SetUp();
    void TearDown();
    
protected:
    void *pool_;
};

//******************************************************************************
class UnitTestHelperNdnNetwork
{
public:
    virtual void NdnSetUp(string &streamAccessPrefix, string &userPrefix);
    virtual void NdnTearDown();
    
    virtual void onInterest(const shared_ptr<const Name>& prefix,
                            const shared_ptr<const Interest>& interest,
                            ndn::Transport& transport);
    virtual void onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix);
    
    virtual void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& data);
    virtual void onTimeout(const shared_ptr<const Interest>& interest);
    
protected:
    bool isFetching_;
    webrtc::ThreadWrapper *fetchingThread_;
    unsigned int nReceivedInterests_, nReceivedData_, nReceivedTimeout_;
    
    ndnrtc::ParamsStruct params_;
    shared_ptr<ndn::Transport> ndnTransport_, ndnReceiverTransport_;
    shared_ptr<Face> ndnFace_, ndnReceiverFace_;
    shared_ptr<KeyChain> ndnKeyChain_;
    shared_ptr<Name> certName_;
    
    // publishes audio or video data packet under the specified prefix by
    // splitting data into a segments and appending
    //  <frame_number>/<segment_number> to the prefix
    void publishMediaPacket(unsigned int dataLen, unsigned char *dataPacket,
                            unsigned int frameNo, unsigned int segmentSize,
                            const string &framePrefix, int freshness,
                            bool mixedSendOrder = false);
    
    // publishes data under the prefix
    void publishData(unsigned int dataLen, unsigned char *dataPacket,
                     const string &prefix, int freshness,
                     int64_t totalSegmentsNum);
    
    void startProcessingNdn();
    void stopProcessingNdn();
    
    static bool fetchThreadFunc(void *obj){
        return ((UnitTestHelperNdnNetwork*)obj)->fetchData();
    }
    bool fetchData();
};

//******************************************************************************
class PacketStream
{
public:
    PacketStream(const char *fileName, const char *rw)
    {
        f_ = fopen(fileName, rw);
    }
    virtual ~PacketStream()
    {
        if (f_)
            fclose(f_);
    }
protected:
    FILE *f_ = NULL;
};

class PacketWriter : public PacketStream
{
public:
    PacketWriter(const char *fileName) : PacketStream(fileName, "w") {}
    ~PacketWriter(){}
    
    void writeData(void *data, unsigned int length) {
        fwrite(data, length, 1, f_);
    }
    
    void synchronize() {
        fflush(f_);
    }
};

class PacketReader : public PacketStream
{
public:
    PacketReader(const char *fileName) : PacketStream(fileName, "r") {}
    ~PacketReader(){}
    bool readBool(bool &value)
    {
        return (1 == fread(&value, sizeof(bool), 1, f_));
    }
    
    bool readData(void *data, unsigned int length)
    {
        return (1 == fread(data, length, 1, f_));
    }
    
    bool readInt32(int &value)
    {
        return (1 == fread(&value, sizeof(int), 1, f_));
    }

    bool readUint32(unsigned int &value)
    {
        return (1 == fread(&value, sizeof(unsigned int), 1, f_));
    }
    
    bool readInt64(int64_t &value)
    {
        return (1 == fread(&value, sizeof(int64_t), 1, f_));
    }
};

// reads YUV video sequence frame by frame
class FrameReader : public PacketReader
{
public:
    FrameReader(const char *fileName) : PacketReader(fileName){}
    ~FrameReader(){}
    
    int readFrame(webrtc::I420VideoFrame &frame)
    {   
        if (!f_)
            return -1;
        
        /*
         How frame is stored in file:
         - render time ms (uint64)
         - width (int)
         - height (int)
         - stride Y (int)
         - stride U (int)
         - stride V (int)
         - Y plane buffer size (int)
         - Y plane buffer (uint8_t*)
         - U plane buffer size (int)
         - U plane buffer (uint8_t*)
         - V plane buffer size (int)
         - V plane buffer (uint8_t*)
         */
        int64_t renderTime;
        int width, height,
        stride_y, stride_u, stride_v,
        size_y, size_u, size_v;
        
        bool res = true;
        
        res &= readInt64(renderTime);
        if (!res) return -1;
        
        res &= readInt32(width);
        if (!res) return -1;
        
        res &= readInt32(height);
        if (!res) return -1;
        
        res &= readInt32(stride_y);
        if (!res) return -1;
        
        res &= readInt32(stride_u);
        if (!res) return -1;
        
        res &= readInt32(stride_v);
        if (!res) return -1;
        
        res &= readInt32(size_y);
        if (!res) return -1;
        
        uint8_t *buffer_y = (uint8_t*)malloc(size_y);
        res &= readData(buffer_y, size_y*sizeof(uint8_t));
        if (!res) return -1;
        
        res &= readInt32(size_u);
        if (!res) return -1;
        
        uint8_t *buffer_u = (uint8_t*)malloc(size_u);
        res &= readData(buffer_u, size_u*sizeof(uint8_t));
        if (!res) return -1;
        
        res &= readInt32(size_v);
        if (!res) return -1;
        
        uint8_t *buffer_v = (uint8_t*)malloc(size_v);
        res &= readData(buffer_v, size_v*sizeof(uint8_t));
        if (!res) return -1;        
                
        if (-1 == frame.CreateFrame(size_y, buffer_y,
                                    size_u, buffer_u,
                                    size_v, buffer_v,
                                    width, height,
                                    stride_y, stride_u, stride_v))
        {
            LOG_NDNERROR("couldn't create frame");
            return -1;
        }
        
        frame.set_render_time_ms(renderTime);
        frame.set_timestamp(renderTime);
        
        return 0;
    }
};

// writes YUV video sequence frame by frame
class FrameWriter : public PacketWriter
{
public:
    FrameWriter(const char *fileName) : PacketWriter(fileName){}
    ~FrameWriter(){}
    
    void writeFrame(webrtc::I420VideoFrame &frame)
    {
        /*
         How frame is stored in file:
         - render time ms (uint64)
         - width (int)
         - height (int)
         - stride Y (int)
         - stride U (int)
         - stride V (int)
         - Y plane buffer size (int)
         - Y plane buffer (uint8_t*)
         - U plane buffer size (int)
         - U plane buffer (uint8_t*)
         - V plane buffer size (int)
         - V plane buffer (uint8_t*)
         */
        int64_t renderTime = frame.render_time_ms();
        int width = frame.width(), height = frame.height();
        int strideY = frame.stride(webrtc::kYPlane),
        strideU = frame.stride(webrtc::kUPlane),
        strideV = frame.stride(webrtc::kVPlane);
        
        writeData(&renderTime, sizeof(renderTime));
        writeData(&width, sizeof(width));
        writeData(&height, sizeof(height));
        writeData(&strideY, sizeof(strideY));
        writeData(&strideU, sizeof(strideU));
        writeData(&strideV, sizeof(strideV));
        
        int sizeY = frame.allocated_size(webrtc::kYPlane),
            sizeU = frame.allocated_size(webrtc::kUPlane),
        sizeV = frame.allocated_size(webrtc::kVPlane);
        
        writeData(&sizeY, sizeof(sizeY));
        writeData(frame.buffer(webrtc::kYPlane), sizeY);
        
        writeData(&sizeU, sizeof(sizeU));
        writeData(frame.buffer(webrtc::kUPlane), sizeU);
        
        writeData(&sizeV, sizeof(sizeV));
        writeData(frame.buffer(webrtc::kVPlane), sizeV);
        
        synchronize();
    }
};

// writes encoded video sequence frame by frame
class EncodedFrameWriter : public PacketWriter
{
public:
    EncodedFrameWriter(const char *fileName):PacketWriter(fileName){}
    ~EncodedFrameWriter(){}
    
    int writeFrame(webrtc::EncodedImage &frame,
                   ndnrtc::PacketData::PacketMetadata &metadata)
    {
        if (!f_)
            return -1;
        
        /*
         How frame is stored in file:
         - size_
         - length_
         - encodedWidth_
         - encodedHeight_
         - timestamp_
         - capture_time_ms_
         - frameType_
         - completeFrame_
         - metadata
         */
        
        writeData(&(frame._size), sizeof(frame._size));
        writeData(&(frame._length), sizeof(frame._length));
        writeData(&(frame._encodedWidth), sizeof(frame._encodedWidth));
        writeData(&(frame._encodedHeight), sizeof(frame._encodedHeight));
        writeData(&(frame._timeStamp), sizeof(frame._timeStamp));
        writeData(&(frame.capture_time_ms_), sizeof(frame.capture_time_ms_));
        writeData(&(frame._frameType), sizeof(frame._frameType));
        writeData(&(frame._completeFrame), sizeof(frame._completeFrame));
        writeData(&metadata, sizeof(metadata));
        writeData(frame._buffer, frame._length);
        
        synchronize();
        
        return 0;
    }
};

// reads encoded video sequence frame by frame
class EncodedFrameReader : public PacketReader
{
public:
    EncodedFrameReader(const char *fileName) : PacketReader(fileName){}
    ~EncodedFrameReader(){}
    
    int readFrame(webrtc::EncodedImage &frame,
                  ndnrtc::PacketData::PacketMetadata &metadata)
    {
        if (!f_)
            return -1;
        
        /*
         How frame is stored in file:
         - size_
         - length_
         - encodedWidth_
         - encodedHeight_
         - timestamp_
         - capture_time_ms_
         - frameType_
         - completeFrame_
         - metadata
         */
        
        bool res = true;
        int size, length, width, height, frameType;
        bool completeFrame;
        int64_t timestamp, captureTime;
        
        res &= readInt32(size);
        if (!res) return -1;
        
        res &= readInt32(length);
        if (!res) return -1;
        
        res &= readInt32(width);
        if (!res) return -1;
        
        res &= readInt32(height);
        if (!res) return -1;
        
        res &= readInt64(timestamp);
        if (!res) return -1;
        
        res &= readInt64(captureTime);
        if (!res) return -1;
        
        res &= readInt32(frameType);
        if (!res) return -1;
        
        res &= readBool(completeFrame);
        if (!res) return -1;
        
        res &= readData(&metadata, sizeof(metadata));
        if (!res) return -1;
        
        frame._buffer = (uint8_t*)realloc((void*)frame._buffer, length);
        
        res &= readData(frame._buffer, length);
        if (!res) return -1;
        
        frame._length = length;
        frame._size = size;
        frame._encodedWidth = width;
        frame._encodedHeight = height;
        frame._timeStamp = timestamp;
        frame.capture_time_ms_ = captureTime;
        frame._frameType = (webrtc::VideoFrameType)frameType;
        frame._completeFrame = completeFrame;

        return 0;
    }
};

// reads audio sequence sample by sample
class AudioReader : public PacketReader
{
public:
    AudioReader(const char *fileName) : PacketReader(fileName){}
    ~AudioReader(){}
    
    int readPacket(ndnrtc::NdnAudioData::AudioPacket &packet)
    {
        if (!f_)
            return -1;
        
        /*
         How frame is stored in file:
         - isRTCP (bool)
         - timestamp (int64_t)
         - audio data lenght (unsigned int)
         - audio data (unsigned char*)
         */
        bool res = true;
        
        res &= readBool(packet.isRTCP_);
        if (!res) return -1;
        
        res &= readInt64(packet.timestamp_);
        if (!res) return -1;
        
        res &= readUint32(packet.length_);
        if (!res) return -1;
        
        res &= readData(packet.data_, packet.length_);
        if (!res) return -1;
        
        return 0;
    }
};

// writes audio sequence sample by sample
class AudioWriter : public PacketWriter
{
public:
    AudioWriter(const char *fileName) : PacketWriter(fileName){}
    ~AudioWriter(){}
    
    void writePacket(ndnrtc::NdnAudioData::AudioPacket &packet)
    {
        /*
         How frame is stored in file:
         - isRTCP (bool)
         - timestamp (int64_t)
         - audio data lenght (unsigned int)
         - audio data (unsigned char*)
         */
        
        writeData(&packet.isRTCP_, sizeof(packet.isRTCP_));
        writeData(&packet.timestamp_, sizeof(packet.timestamp_));
        writeData(&packet.length_, sizeof(packet.length_));
        writeData(packet.data_, packet.length_);
        
        synchronize();
    }
};

#endif
