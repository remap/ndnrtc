//
//  full-loppback-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include "test-common.h"
#include "sender-channel.h"
#include "receiver-channel.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME, NdnLoggerDetailLevelAll));


class TestReceiverChannel : public NdnReceiverChannel, public IEncodedFrameConsumer
{
public:
    TestReceiverChannel(const ParamsStruct &p, const ParamsStruct &ap):
    NdnReceiverChannel(p, ap)
    {
        localRender_->setObserver(this);
        decoder_->setObserver(this);
        receiver_->setObserver(this);
        
        receiver_->setFrameConsumer(this);
        decoder_->setFrameConsumer(localRender_.get());
    }
    
    void onEncodedFrameDelivered(webrtc::EncodedImage &encodedImage)
    {
        // check timestamp with the last one receiveed
        if (lastTimeStamp_)
        {
            EXPECT_LE(lastTimeStamp_, encodedImage.capture_time_ms_);
            
            if (lastTimeStamp_ >  encodedImage.capture_time_ms_)
                nMisordered_++;
        }
        else
            lastTimeStamp_ = encodedImage.capture_time_ms_;
        
        // pass frame further
        decoder_->onEncodedFrameDelivered(encodedImage);
    }
    
    uint64_t lastTimeStamp_ = 0;
    unsigned int nMisordered_ = 0;
};

TEST(LoopbackTests, Transmission)
{
    ParamsStruct p = DefaultParams;
    ParamsStruct audioP = DefaultParamsAudio;
    
    NdnSenderChannel *sc = new NdnSenderChannel(p, audioP);
    TestReceiverChannel *rc = new TestReceiverChannel(p, audioP);
    
    EXPECT_EQ(0, sc->init());
    EXPECT_EQ(0, rc->init());
    
    EXPECT_EQ(0, sc->startTransmission());
    WAIT(200);
    EXPECT_EQ(RESULT_OK, rc->startTransmission());
    
    bool f=  false;
//    EXPECT_TRUE_WAIT(f, 10000);
    WAIT(10000);
    
    EXPECT_EQ(0, sc->stopTransmission());
    EXPECT_EQ(RESULT_OK,rc->stopTransmission());
    
    EXPECT_EQ(0,rc->nMisordered_);
    
    delete sc;
    delete rc;
}
#if 0
TEST(LoopbackTests, Transmission)
{
    ParamsStruct p = DefaultParams;
    ParamsStruct audioP = DefaultParamsAudio;
    
    p.loggingLevel = NdnLoggerDetailLevelDebug;
#if 0
    p.producerId = "lioncub";
    p.ndnHub = "ndn/caida.org";
#else
    p.producerId = "nibbler";
    p.ndnHub = "ndn/ucla.edu/apps";
#endif
    p.encodeWidth = 640;
    p.encodeHeight = 480;
    p.renderWidth = 640;
    p.renderHeight = 480;
    audioP.producerId = p.producerId;
    audioP.ndnHub = p.ndnHub;
    
    TestReceiverChannel *rc = new TestReceiverChannel(p, audioP);
    
    EXPECT_EQ(0, rc->init());
    
    EXPECT_EQ(RESULT_OK, rc->startTransmission());
    
    bool f=  false;
    //    EXPECT_TRUE_WAIT(f, 10000);
    WAIT(100000);
    
    EXPECT_EQ(RESULT_OK,rc->stopTransmission());
    EXPECT_EQ(0,rc->nMisordered_);
    
    delete rc;
}
#endif
