//
//  fec-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 5/12/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"

//#include <ndn-fec/fec_encode.h>
//#include <ndn-fec/fec_decode.h>
#include <ndn-fec/fec_common.h>
extern "C" {
#include "ndn-fec/lib_common/of_openfec_api.h"
}

// FEC classes declarations
class FecEncode {
    of_session_t*    ses;
    of_status_t      ret;
    of_codec_id_t    codec_id;
    of_parameters_t* params;
    
protected:
    int n, k, symbol_len;
    int createCodecInstance();
    int setFecParameters(void* my_params);
    int buildRepairSymbol(void** encoding_symb_tab);
    
public:
    FecEncode(of_codec_id_t codec_id, uint32_t n, uint32_t k, int symbol_len);
    virtual ~FecEncode();
    virtual int encode(char* data, char* parity) = 0;
};

class Rs28Encode : public FecEncode {
    of_rs_parameters_t *my_params;
    RS28INFO info;
public:
    Rs28Encode (uint32_t n, uint32_t k, int symbol_len);
    Rs28Encode (RS28INFO info);
    ~Rs28Encode();
    
    int encode(char* data, char* parity);
    int encode();
};

class FecDecode {
    of_session_t  *ses;
    of_status_t   ret;
    int context;
    of_codec_id_t codec_id;
    of_parameters_t* params;
    int counter;
    
    
protected:
    int n, k, symbol_len;
    char* p_r_list;
    int createCodecInstance();
    int setFecParameters(void* my_params);
    int decodeSymbol(void** decoding_symb_tab, char* r_list);
    int getDecodedData(void** decoding_symb_tab);
    void printFlags();
    void setRecoveredFlag(UINT32 esp);
public:
    FecDecode (of_codec_id_t codec_id, uint32_t n, uint32_t k, int symbol_len);
    virtual int decode(char* data, char* parity, char* r_list) = 0;
    
    
private:
    static void* source_cb(void* context, UINT32 size, UINT32 esi);
    static void* repair_cb(void* context, UINT32 size, UINT32 esi);
};

class Rs28Decode : public FecDecode {
    of_rs_parameters_t *my_params;
    RS28INFO info;
    
public:
    Rs28Decode (uint32_t n, uint32_t k, int symbol_len);
    Rs28Decode(RS28INFO info);
    int decode(char* data, char* parity, char* r_list);
    int decode();
};

//

::testing
::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

TEST(TestFEC, TestEncodeDecode)
{
    webrtc::EncodedImage *testFrame = NdnRtcObjectTestHelper::loadEncodedFrame();
    
    unsigned int nSegments = 5;
    double parityRatio = 2;
    unsigned int nParitySegments = ceil(parityRatio*nSegments);
    unsigned int segmentSize = testFrame->_length/nSegments;
    
    PacketData::PacketMetadata meta;
    meta.packetRate_ = 16.;
    meta.timestamp_ = NdnRtcUtils::millisecondTimestamp();
    meta.unixTimestamp_ = NdnRtcUtils::unixTimestamp();
    
    NdnFrameData frameData(*testFrame, meta);
    Segmentizer::SegmentList segments;
    Segmentizer::segmentize(frameData, segments, segmentSize);
    
    nSegments = segments.size();
    
    Rs28Encode enc(nSegments+nParitySegments, nSegments, segmentSize);
    
    unsigned char* parityData = (unsigned char*)malloc(segmentSize*nParitySegments);
    memset(parityData, 0, segmentSize*nParitySegments);
    
    EXPECT_EQ(0, enc.encode((char*)frameData.getData(), (char*)parityData));
    
    // now decode
    unsigned char* segList = (unsigned char*)malloc(nSegments+nParitySegments);
    memset(segList, '1', nSegments+nParitySegments);
    
    unsigned char* packetDataWithParity = (unsigned char*)malloc((nSegments+nParitySegments)*segmentSize);
    memset(packetDataWithParity, 0, (nSegments+nParitySegments)*segmentSize);
    
    memcpy(packetDataWithParity, frameData.getData(), frameData.getLength());
    memcpy(packetDataWithParity+nSegments*segmentSize, parityData, nParitySegments*segmentSize);
    
    // now remove some segment
    int nSegmentToLose = NdnRtcObjectTestHelper::randomInt(0, nSegments);
    
    segList[nSegmentToLose] = '0';
    memset(packetDataWithParity+nSegmentToLose*segmentSize, 0, segmentSize);
    
    RS28INFO rsInfo;
    rsInfo.data_segment_num = nSegments;
    rsInfo.parity_segment_num = nParitySegments;
    rsInfo.one_segment_size = segmentSize;
    rsInfo.buf = (char*)packetDataWithParity;
    rsInfo.r_list = (char*)segList;
    
    Rs28Decode dec(rsInfo);
    
    EXPECT_EQ(1, dec.decode());
    EXPECT_EQ('2', segList[nSegmentToLose]);
    
    // now check frame
    NdnFrameData recoveredData(frameData.getLength(), packetDataWithParity);
    webrtc::EncodedImage recoveredFrame;
    recoveredData.getFrame(recoveredFrame);
    
    NdnRtcObjectTestHelper::checkFrames(testFrame, &recoveredFrame);
    
    delete testFrame;
}
#if 0
TEST(TestFEC, TestEncodeDecodePacket)
{
    webrtc::EncodedImage *testFrame = NdnRtcObjectTestHelper::loadEncodedFrame();
    
    unsigned int nSegments = 30;
    double parityRatio = 0.5;
    unsigned int nParitySegments = ceil(parityRatio*nSegments);
    unsigned int segmentSize = testFrame->_length/nSegments;
    
    PacketData::PacketMetadata meta;
    meta.packetRate_ = 16.;
    meta.timestamp_ = NdnRtcUtils::millisecondTimestamp();
    meta.unixTimestamp_ = NdnRtcUtils::unixTimestamp();
    
    NdnFrameData frameData(*testFrame, meta);
    Segmentizer::SegmentList segments;
    Segmentizer::segmentize(frameData, segments, segmentSize);
    
    nSegments = segments.size();
    
    FrameParityData parityData;
    
    EXPECT_EQ(RESULT_OK, parityData.initFromPacketData(frameData, parityRatio, nSegments, segmentSize));
    
    // now decode
    unsigned char* segList = (unsigned char*)malloc(nSegments+nParitySegments);
    memset(segList, '1', nSegments+nParitySegments);
    
    unsigned char* packetDataWithParity = (unsigned char*)malloc((nSegments+nParitySegments)*segmentSize);
    memset(packetDataWithParity, 0, (nSegments+nParitySegments)*segmentSize);
    
    memcpy(packetDataWithParity, frameData.getData(), frameData.getLength());
    memcpy(packetDataWithParity+nSegments*segmentSize, parityData.getData(), parityData.getLength());
    
    // now remove some segment
    int nSegmentToLose = NdnRtcObjectTestHelper::randomInt(0, nSegments);
    
    segList[nSegmentToLose] = '0';
    memset(packetDataWithParity+nSegmentToLose*segmentSize, 0, segmentSize);
    
    RS28INFO rsInfo;
    rsInfo.data_segment_num = nSegments;
    rsInfo.parity_segment_num = nParitySegments;
    rsInfo.one_segment_size = segmentSize;
    rsInfo.buf = (char*)packetDataWithParity;
    rsInfo.r_list = (char*)segList;
    
    Rs28Decode dec(rsInfo);
    
    EXPECT_EQ(1, dec.decode());
    EXPECT_EQ('2', segList[nSegmentToLose]);
    
    // now check frame
    NdnFrameData recoveredData(frameData.getLength(), packetDataWithParity);
    webrtc::EncodedImage recoveredFrame;
    recoveredData.getFrame(recoveredFrame);
    
    NdnRtcObjectTestHelper::checkFrames(testFrame, &recoveredFrame);
    
    delete testFrame;
}
#endif