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

#include "test-common.h"
#include <sys/time.h>
#include "simple-log.h"
#include "ndnrtc-namespace.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

using namespace ndnrtc;

//******************************************************************************
int64_t millisecondTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    int64_t ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
    
    return ticks;
};

//******************************************************************************
void CocoaTestEnvironment::SetUp(){
    printf("[setting up cocoa environment]\n");
    pool_ = (void*)[[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
};

void CocoaTestEnvironment::TearDown(){
    [(NSAutoreleasePool*)pool_ release];
    printf("[tear down cocoa environment]\n");
};

//******************************************************************************
void UnitTestHelperNdnNetwork::NdnSetUp(string &streamAccessPrefix, string &userPrefix)
{
    nReceivedInterests_ = 0;
    nReceivedData_ = 0;
    nReceivedTimeout_ = 0;

    shared_ptr<ndn::Transport::ConnectionInfo>
        connInfo(new ndn::TcpTransport::ConnectionInfo(params_.host,
                                                       params_.portNum));
    ndnTransport_.reset(new TcpTransport());
    ndnFace_.reset(new Face(ndnTransport_, connInfo));
    ASSERT_NO_THROW(
                    ndnFace_->registerPrefix(Name(streamAccessPrefix.c_str()),
                                             bind(&UnitTestHelperNdnNetwork::onInterest, this, _1, _2, _3),
                                             bind(&UnitTestHelperNdnNetwork::onRegisterFailed, this, _1));
                    );
    shared_ptr<ndn::Transport::ConnectionInfo>
    connInfo2(new TcpTransport::ConnectionInfo(params_.host, params_.portNum));
    ndnReceiverTransport_.reset(new ndn::TcpTransport());
    ndnReceiverFace_.reset(new Face(ndnReceiverTransport_, connInfo2));
    ASSERT_NO_THROW(
    ndnReceiverFace_->registerPrefix(Name((streamAccessPrefix+"/rcv").c_str()),
                                     bind(&UnitTestHelperNdnNetwork::onInterest,
                                          this, _1, _2, _3),
                                     bind(&UnitTestHelperNdnNetwork::onRegisterFailed,
                                          this, _1));
                    );
    ndnKeyChain_ = NdnRtcNamespace::keyChainForUser(userPrefix);
    certName_ = NdnRtcNamespace::certificateNameForUser(userPrefix);
}

void UnitTestHelperNdnNetwork::NdnTearDown()
{
    
}

void UnitTestHelperNdnNetwork::onInterest(const shared_ptr<const Name>& prefix,
                                          const shared_ptr<const Interest>& interest,
                                          ndn::Transport& transport)
{
    nReceivedInterests_++;
}

void UnitTestHelperNdnNetwork::
onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
{
    FAIL();
}

void UnitTestHelperNdnNetwork::onData(const shared_ptr<const Interest>& interest,
                                      const shared_ptr<Data>& data)
{
    nReceivedData_++;
}

void UnitTestHelperNdnNetwork::
onTimeout(const shared_ptr<const Interest>& interest)
{
    LOG_TRACE("got timeout for interest: %s", interest->getName().toUri().c_str());
    nReceivedTimeout_++;
}

void UnitTestHelperNdnNetwork::
publishMediaPacket(unsigned int dataLen, unsigned char *dataPacket,
                   unsigned int frameNo, unsigned int segmentSize,
                   const string &framePrefix, int freshness,
                   bool mixedSendOrder)
{
    unsigned int fullSegmentsNum = dataLen/segmentSize;
    unsigned int totalSegmentsNum = (dataLen - fullSegmentsNum*segmentSize)?
        fullSegmentsNum+1:fullSegmentsNum;
    
    unsigned int lastSegmentSize = dataLen - fullSegmentsNum*segmentSize;
    vector<int> segmentsSendOrder;
    
    Name prefix(framePrefix.c_str());
    shared_ptr<const vector<unsigned char>> frameNumberComponent =
        NdnRtcNamespace::getNumberComponent(frameNo);
    
    prefix.addComponent(*frameNumberComponent);
    
    // setup send order for segments
    for (int i = 0; i < (int)totalSegmentsNum; i++)
        segmentsSendOrder.push_back(i);
    
    if (mixedSendOrder)
        random_shuffle(segmentsSendOrder.begin(), segmentsSendOrder.end());
    
    for (int i = 0; i < (int)totalSegmentsNum; i++)
    {
        unsigned int segmentIdx = segmentsSendOrder[i];
        unsigned char *segmentData = dataPacket+segmentIdx*segmentSize;
        unsigned int
        dataSize = (segmentIdx == totalSegmentsNum -1)?lastSegmentSize:
                                                        segmentSize;
        
        if (dataSize > 0)
        {
            Name segmentPrefix = prefix;
            segmentPrefix.appendSegment(segmentIdx);
           publishData(dataSize, segmentData,
                       segmentPrefix.toUri(), freshness,
                       totalSegmentsNum);
        } // if
    } // for
    
}
void UnitTestHelperNdnNetwork::publishData(unsigned int dataLen,
                                           unsigned char *dataPacket,
                                           const string &prefix, int freshness,
                                           int64_t totalSegmentsNum)
{
    Data data(prefix);
    
    data.getMetaInfo().setFreshnessSeconds(freshness);
    data.getName().appendFinalSegment(totalSegmentsNum-1);
    data.getMetaInfo().setTimestampMilliseconds(millisecondTimestamp());
    data.setContent(dataPacket, dataLen);
    
    ndnKeyChain_->sign(data, *certName_);
    
    ASSERT_TRUE(ndnTransport_->getIsConnected());
    
    SignedBlob encodedData = data.wireEncode();
    ndnTransport_->send(*encodedData);
//    NdnLogger::log(__NDN_FNAME__, NdnLoggerLevelTrace, "published %s",prefix.c_str());
    LOG_TRACE("published %s", prefix.c_str());
}

void UnitTestHelperNdnNetwork::startProcessingNdn()
{
    fetchingThread_ = webrtc::ThreadWrapper::CreateThread(fetchThreadFunc, this);

    unsigned int tid = 1;
    ASSERT_NE(0,fetchingThread_->Start(tid));
    isFetching_ = true;
}
void UnitTestHelperNdnNetwork::stopProcessingNdn()
{
    isFetching_ = false;
    fetchingThread_->SetNotAlive();
    fetchingThread_->Stop();
    
    delete fetchingThread_;
}

bool UnitTestHelperNdnNetwork::fetchData()
{
    ndnFace_->processEvents();
    WAIT(10);
    
    return isFetching_;
}
