//
//  interest-queue.cc
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#include <tr1/unordered_set>

#include "test-common.h"
#include "interest-queue.h"
#include "ndnrtc-namespace.h"


//::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

using namespace ndnrtc::new_api;

class InterestQueueTests : public UnitTestHelperNdnNetwork,
public NdnRtcObjectTestHelper
{
public:
    InterestQueueTests():
    accessCs_(*webrtc::CriticalSectionWrapper::CreateCriticalSection()){}
    
    void SetUp()
    {
        accessPrefix_ = "/ndn/ndnrtc/test/access";
        userPrefix_ = "/ndn/ndnrtc/test/user";
        
        params_ = DefaultParams;
        UnitTestHelperNdnNetwork::NdnSetUp(accessPrefix_, userPrefix_);
        
        fetchChannel_.reset(new FetchChannelMock(ENV_LOGFILE));
        queue_ = new InterestQueue(fetchChannel_, ndnReceiverFace_);
    }
    
    void TearDown()
    {
        UnitTestHelperNdnNetwork::NdnTearDown();
        
        delete queue_;
    }
    
    void
    onInterest(const shared_ptr<const Name>& prefix,
               const shared_ptr<const Interest>& interest,
               ndn::Transport& transport)
    {
        UnitTestHelperNdnNetwork::onInterest(prefix, interest, transport);
        
        Name userPrefix(userPrefix_);
        
        if (interest->getName().getComponentCount() > userPrefix.getComponentCount())
        {
            Name interestNoComp = interest->getName().getSubName(userPrefix.getComponentCount(), 1);
            int interestId = NdnRtcUtils::intFromComponent(interestNoComp[0]);
            
            EXPECT_NE(pendingInterests_.end(), pendingInterests_.find(interestId));
            pendingInterests_.erase(interestId);
        }
    }
    
    void
    onRegisterFailed(const ptr_lib::shared_ptr<const Name>& prefix)
    {
        cout << "failed registering " << prefix << endl;
        
        UnitTestHelperNdnNetwork::onRegisterFailed(prefix);
    }
    
    void
    onTimeout(const shared_ptr<const Interest>& interest)
    {
        UnitTestHelperNdnNetwork::onTimeout(interest);
        std::cout << "interest timed out " << interest->getName() << endl;
    }
    
    
protected:
    class Worker {
    public:
        Worker():
        threadWrapper_(*webrtc::ThreadWrapper::CreateThread(InterestQueueTests::workerRoutine, this)){}
        void *userObj_;
        webrtc::ThreadWrapper &threadWrapper_;
        unsigned int tid_;
        int startId_, stopId_, curId_;
        int64_t priority_;
    };
    
    int interestLifetimeMs_ = 2000;
    std::string accessPrefix_, userPrefix_;
    std::tr1::unordered_set<int> pendingInterests_;
    InterestQueue *queue_;
    shared_ptr<FetchChannel> fetchChannel_;
    
    webrtc::CriticalSectionWrapper &accessCs_;
    
    bool isRunning_;
    std::vector<shared_ptr<Worker>> workers_;
    
    int startWorker(int startId, int stopId, int64_t priority)
    {
        shared_ptr<Worker> w(new Worker());
        w->userObj_ = this;
        w->startId_ = startId;
        w->stopId_ = stopId;
        w->curId_ = w->startId_;
        w->priority_ = priority;
        
        w->threadWrapper_.Start(w->tid_);
        workers_.push_back(w);
        
        return 0;
    }
    
    void stopAllWorkers()
    {
        std::vector<shared_ptr<Worker>>::iterator it;
        isRunning_ = false;
        
        for (it = workers_.begin(); it != workers_.end(); ++it)
        {
            (*it)->threadWrapper_.Stop();
        }
        
        workers_.clear();
    }
    
    static bool
    workerRoutine(void* worker)
    {
        Worker* w = (Worker*)worker;
        
        return ((InterestQueueTests*)w->userObj_)->runWorker(w);
    }
    
    bool runWorker(Worker* w)
    {
        int interestId = w->curId_++;
        Name prefix(userPrefix_);
        prefix.append(NdnRtcUtils::componentFromInt(interestId));
        Interest i(prefix, interestLifetimeMs_);
        
        accessCs_.Enter();
        pendingInterests_.insert(interestId);
        accessCs_.Leave();
        
        queue_->enqueueInterest(i,
                                Priority::fromArrivalDelay(w->priority_),
                                bind(&InterestQueueTests::onData, this, _1, _2),
                                bind(&InterestQueueTests::onTimeout, this, _1));
        return (w->curId_ < w->stopId_) && isRunning_;
    }
    
};

TEST_F(InterestQueueTests, Test)
{
    // 1st - register prefix
    ndnFace_->registerPrefix(Name(userPrefix_.c_str()),
                             bind(&InterestQueueTests::onInterest, this, _1, _2, _3),
                             bind(&InterestQueueTests::onRegisterFailed, this, _1));
    
    
    // number of concurrent threads issuing interests
    int nWorkers = 10;
    int nInterestsPerWorker = 30;
    int staticPriority = 1000;
    
    isRunning_ = true;
    
    for (int i = 0; i < nWorkers; i++)
    {
        if (i == nWorkers -1)
            staticPriority = 0;
        
        startWorker(i*nInterestsPerWorker,
                    i*nInterestsPerWorker+nInterestsPerWorker,
                    staticPriority-(double)staticPriority/(double)nWorkers);
    }
    
    // process events for receiving all the interests
    int waitMs = 1000;
    int delta = 5;
    
    startProcessingNdn();
    WAIT(1000);
    stopProcessingNdn();
    
    stopAllWorkers();

    
    EXPECT_EQ(0, pendingInterests_.size());
}