//
//  pipeliner-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 3/3/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "pipeliner.h"
#include "ndnrtc-utils.h"
#include "ndnrtc-namespace.h"
#include "sender-channel.h"

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;

class PipelinerFetchChannelMock : public FetchChannelMock
{
public:
    PipelinerFetchChannelMock(string logfile):FetchChannelMock(logfile){}
    
    void
    setFrameBuffer(const shared_ptr<ndnrtc::new_api::FrameBuffer>& frameBuffer)
    { frameBuffer_ = frameBuffer; }
    
    void
    setBufferEstimator(const shared_ptr<BufferEstimator>& bufferEstimator)
    { bufferEstimator_= bufferEstimator; }
    
    void
    setChaseEstimation(const shared_ptr<ChaseEstimation>& chaseEstimation)
    { chaseEstimation_ = chaseEstimation; }
    
    void
    setAssembler(const shared_ptr<IPacketAssembler>& ndnAssembler)
    { packetAssembler_ = ndnAssembler; }
    
    void
    setInterestQueue(const shared_ptr<InterestQueue>& interestQueue)
    { interestQueue_ = interestQueue; }
};

class PipelinerTests : public UnitTestHelperNdnNetwork,
public NdnRtcObjectTestHelper,
public IPacketAssembler
{
public:
    void SetUp()
    {
        params_ = DefaultParams;
        params_.captureDeviceId = 1;
        
        shared_ptr<string> accessPref = NdnRtcNamespace::getStreamKeyPrefix(params_);
        shared_ptr<string> userPref = NdnRtcNamespace::getStreamFramePrefix(params_);
        UnitTestHelperNdnNetwork::NdnSetUp(*accessPref, *userPref);
        
        PipelinerFetchChannelMock *mock = new PipelinerFetchChannelMock("");//ENV_LOGFILE);
        mock->setParameters(params_);
        fetchChannel_.reset(mock);
        
        frameBuffer_.reset(new ndnrtc::new_api::FrameBuffer(fetchChannel_));
        interestQueue_.reset(new InterestQueue(fetchChannel_, ndnFace_));
        bufferEstimator_.reset(new BufferEstimator());
        chaseEstimation_.reset(new ChaseEstimation());
        packetAssembler_.reset(this);
        
        mock->setAssembler(packetAssembler_);
        mock->setFrameBuffer(frameBuffer_);
        mock->setBufferEstimator(bufferEstimator_);
        mock->setChaseEstimation(chaseEstimation_);
        mock->setInterestQueue(interestQueue_);
    }
    
    void TearDown()
    {
        UnitTestHelperNdnNetwork::NdnTearDown();
    }
    
    void onData(const shared_ptr<const Interest>& interest,
                        const shared_ptr<Data>& data)
    {
        UnitTestHelperNdnNetwork::onData(interest, data);
        
        cout << "new data " << data->getName() << endl;
        
        frameBuffer_->newData(*data);
    }
    void onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);
        cout << "timeout" << endl;
    }
    
    ndn::OnData getOnDataHandler()
    {
        return bind(&PipelinerTests::onData, this, _1, _2);
    }
    ndn::OnTimeout getOnTimeoutHandler()
    {
        return bind(&PipelinerTests::onTimeout, this, _1);
    }
    
protected:
    shared_ptr<const FetchChannel> fetchChannel_;
    shared_ptr<ndnrtc::new_api::FrameBuffer> frameBuffer_;
    shared_ptr<BufferEstimator> bufferEstimator_;
    shared_ptr<ChaseEstimation> chaseEstimation_;
    shared_ptr<InterestQueue> interestQueue_;
    shared_ptr<IPacketAssembler> packetAssembler_;
    shared_ptr<NdnSenderChannel> sendChannel_;
    
    void
    startPublishing()
    {
        sendChannel_.reset(new NdnSenderChannel(params_, DefaultParamsAudio));
        
        EXPECT_TRUE(RESULT_NOT_FAIL(sendChannel_->init()));
        EXPECT_TRUE(RESULT_NOT_FAIL(sendChannel_->startTransmission()));
    }
    
    void
    stopPublishing()
    {
        EXPECT_EQ(RESULT_OK, sendChannel_->stopTransmission());
    }
};

TEST_F(PipelinerTests, TestCreateDelete)
{
    Pipeliner p(fetchChannel_);
}

TEST_F(PipelinerTests, TestInit)
{
    startPublishing();
    startProcessingNdn();
    
    Pipeliner p(fetchChannel_);
    p.start();
    
    frameBuffer_->init();
    WAIT(10000);

    stopProcessingNdn();
    stopPublishing();
}
