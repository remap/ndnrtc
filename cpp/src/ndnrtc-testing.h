//
//  ndnrtc-testing.h
//  ndnrtc
//
//  Created by Peter Gusev on 4/7/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#ifndef __ndnrtc__ndnrtc_testing__
#define __ndnrtc__ndnrtc_testing__

#include "ndnrtc-common.h"

namespace ndnrtc {
    namespace testing {
        using namespace ndnlog::new_api;
        
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
                    Logger::sharedInstance() << "couldn't create frame" << std::endl;
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
                int size, length, width, height, frameType, timestamp;
                bool completeFrame;
                int64_t captureTime;
                
                res &= readInt32(size);
                if (!res) return -1;
                
                res &= readInt32(length);
                if (!res) return -1;
                
                res &= readInt32(width);
                if (!res) return -1;
                
                res &= readInt32(height);
                if (!res) return -1;
                
                res &= readInt32(timestamp);
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
                writeData(&packet.length_, sizeof(packet.length_));
                writeData(packet.data_, packet.length_);
                
                synchronize();
            }
        };
        
        class ConsumerMock : public ndnrtc::new_api::Consumer
        {
        public:
            ConsumerMock(const ParamsStruct& params,
                         const shared_ptr<new_api::InterestQueue>& interestQueue,
                         string logFile):
            Consumer(params, interestQueue),logFile_(logFile){}
            
            virtual std::string getLogFile() const
            { return logFile_; }
            
            virtual ndnrtc::ParamsStruct
            getParameters() const { return params_; }
            
            void
            setParameters(ParamsStruct p)
            { params_ = p; }
            
        private:
            string logFile_;
            ParamsStruct params_;
        };
    }
}

#endif /* defined(__ndnrtc__ndnrtc_testing__) */
