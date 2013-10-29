//
//  full-loppback-test.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#define NDN_LOGGING
#define NDN_INFO
#define NDN_WARN
#define NDN_ERROR
#define NDN_TRACE

#include "test-common.h"
#include "sender-channel.h"
#include "receiver-channel.h"

using namespace ndnrtc;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

#if 0
class TestReceiverChannel : public NdnReceiverChannel, public IEncodedFrameConsumer
{
public:
    TestReceiverChannel(const ParamsStruct &p):
    NdnReceiverChannel(p)
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
    p.freshness = 5;
    
    NdnSenderChannel *sc = new NdnSenderChannel(p);
    TestReceiverChannel *rc = new TestReceiverChannel(p);
    
    EXPECT_EQ(0, sc->init());
    EXPECT_EQ(0, rc->init());
    
    sc->startTransmission();
    WAIT(200);
    rc->startFetching();
    
    bool f=  false;
//    EXPECT_TRUE_WAIT(f, 10000);
    WAIT(5000);
    
    sc->stopTransmission();
    rc->stopFetching();
    
    EXPECT_EQ(0,rc->nMisordered_);
    
    delete sc;
    delete rc;
}
#endif