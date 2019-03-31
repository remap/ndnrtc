//
// fetch.cpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include "fetch.hpp"

#include <chrono>
#include <mutex>

#include <boost/chrono.hpp>
#include <boost/make_shared.hpp>

#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include "../../include/name-components.hpp"
#include "../../include/simple-log.hpp"
#include "../../include/statistics.hpp"
#include "../../include/helpers/key-chain-manager.hpp"

#include "../../src/interest-queue.hpp"
#include "../../src/pool.hpp"
#include "../../src/proto/ndnrtc.pb.h"
#include "../../src/pipeline.hpp"
#include "../../src/pipeline-control.hpp"
#include "../../src/frame-buffer.hpp"
#include "../../src/network-data.hpp"
#include "../../src/packets.hpp"
#include "../../src/clock.hpp"
#include "../../src/estimators.hpp"

#include "precise-generator.hpp"

using namespace std;
using namespace ndn;
using namespace ndnrtc;
using namespace ndnlog;
using namespace ndnlog::new_api;
using namespace ndnrtc::estimators;

typedef struct _FetchingParams {
    NamespaceInfo prefixInfo_;
    int ppSize_, ppStep_, pbcRate_;
    bool ppAdjustable_, useFec_;
} FetchingParams;

void setupFetching(boost::shared_ptr<KeyChain> keyChain,
                   FetchingParams fetchingParams);
void printStats(boost::shared_ptr<BufferSlot> slot,
                boost::shared_ptr<v4::PipelineControl> ppCtrl,
                boost::shared_ptr<Pool<BufferSlot>> slotPool);

typedef function<void(FetchingParams)> OnMetaProcessed;
vector<boost::shared_ptr<DataRequest>> setupStreamMetaProcessing(FetchingParams, OnMetaProcessed);

//******************************************************************************
// CONSUMER APP VARIABLES
boost::shared_ptr<Face> face;
boost::shared_ptr<helpers::KeyChainManager> keyChainManager;
boost::shared_ptr<RequestQueue> requestQ;
boost::shared_ptr<v4::PipelineControl> pipelineControl;
boost::shared_ptr<Pipeline> pipeline;

void runFetching(boost::asio::io_service &io,
                 std::string output,
                 const ndnrtc::NamespaceInfo& prefixInfo,
                 long ppSize,
                 long ppStep,
                 long pbcRate,
                 bool useFec,
                 bool needRvp,
                 string policyFile)
{
    face = boost::make_shared<ThreadsafeFace>(io);
    keyChainManager =
                boost::make_shared<helpers::KeyChainManager>(face,
                                                             boost::make_shared<KeyChain>(),
                                                             "/localhost/operator", "ndnrtc-consumer",
                                                             policyFile, 3600,
                                                             Logger::getLoggerPtr(AppLog));
    requestQ = boost::make_shared<RequestQueue>(io, face.get());
    requestQ->setLogger(Logger::getLoggerPtr(AppLog));

    FetchingParams params;
    params.prefixInfo_ = prefixInfo;
    params.ppSize_ = (int)ppSize;
    params.ppStep_ = (int)ppStep;
    params.pbcRate_ = (int)pbcRate;
    params.ppAdjustable_ = (ppSize == 0);
    params.useFec_ = useFec;

    // check if need to fetch from live stream
    if (prefixInfo.hasSeqNo_ == false)
    {
        auto metaRequests = setupStreamMetaProcessing(params,
                                bind(setupFetching, keyChainManager->instanceKeyChain(), _1));
        requestQ->enqueueRequests(metaRequests);
    }
    else
        setupFetching(keyChainManager->instanceKeyChain(), params);

    do {
        io.run();
        if (io.stopped())
        {
            LogWarn(AppLog) << "restart io_service" << endl;
            #ifdef BOOST_ASIO_IO_CONTEXT_HPP
            io.restart();
            #else
            io.reset();
            #endif
        }
    } while (!MustTerminate);
}

void setupFetching(boost::shared_ptr<KeyChain> keyChain,
                   FetchingParams fp)
{
    LogInfo(AppLog)  << "fetching from " << fp.prefixInfo_.getPrefix(NameFilter::Stream)
                     << endl;
    LogDebug(AppLog) << " start seq " << fp.prefixInfo_.sampleNo_
                     << " pp-sz " << fp.ppSize_ << (fp.ppAdjustable_ ? " (adj)" : " (no-adj)")
                     << " pp-step " << fp.ppStep_
                     << " pbc-rate " << fp.pbcRate_
                     << (fp.useFec_ ? "use-fec" : "no-fec")
                     << endl;

    // setup Pipeline
    auto slotPool = boost::make_shared<Pool<BufferSlot>>(500);
    pipeline = boost::make_shared<Pipeline>(requestQ,
                                            [slotPool]()->boost::shared_ptr<IPipelineSlot>{
                                                return slotPool->pop();
                                            },
                                            fp.prefixInfo_.getPrefix(NameFilter::Stream),
                                            fp.prefixInfo_.sampleNo_, fp.ppStep_);

    // setup PipelineControl
    pipelineControl = boost::make_shared<v4::PipelineControl>(fp.ppSize_);

    pipelineControl->onNewRequest.connect(bind(&Pipeline::pulse, pipeline));
    pipelineControl->onSkipPulse.connect([](){
        LogDebug(AppLog) << "pipeline-control: pulse skipped" << endl;
    });

    // TEMPORARY: onNewSlot will be connected to a buffer
    pipeline->onNewSlot.connect([slotPool](boost::shared_ptr<IPipelineSlot> s){
        auto bufferSlot = dynamic_pointer_cast<BufferSlot>(s);

        bufferSlot->addOnNeedData([&](IPipelineSlot *sl, std::vector<boost::shared_ptr<DataRequest>> missingR)
                                  {
                                      sl->setRequests(missingR);
                                      requestQ->enqueueRequests(missingR, boost::make_shared<DeadlinePriority>(REQ_DL_PRI_RTX));
                                  });

        bufferSlot->subscribe(PipelineSlotState::Ready,
                              [bufferSlot, slotPool](const IPipelineSlot *sl, const DataRequest&)
                              {
                                  LogDebug(AppLog) << "slot "
                                                   << bufferSlot->getNameInfo().getSuffix(NameFilter::Sample)
                                                   << " (" << bufferSlot->getNameInfo().sampleNo_ << ") assembled in "
                                                   << bufferSlot->getLongestDrd()/1000 << "ms"
                                                   << endl;

                                  if (!(Logger::getLogger(AppLog).getLogLevel() > NdnLoggerDetailLevelDefault && AppLog == ""))
                                      printStats(bufferSlot, pipelineControl, slotPool);

                                  pipelineControl->pulse();
                                  slotPool->push(bufferSlot);
                              });
        bufferSlot->subscribe(PipelineSlotState::Unfetchable,
                              [bufferSlot, slotPool](const IPipelineSlot *sl, const DataRequest& dr)
                              {
                                  LogWarn(AppLog) << "slot "
                                                  << bufferSlot->getNameInfo().getSuffix(NameFilter::Sample)
                                                  << " (" << bufferSlot->getNameInfo().sampleNo_
                                                  << ") unfetchable. reason " << dr.getStatus()
                                                  << endl;

                                  // keep going if it's a timeout
                                  if (dr.getStatus() == DataRequest::Status::Timeout)
                                  {
                                      pipelineControl->pulse();
                                      slotPool->push(bufferSlot);
                                  }
                              });
    });

    pipeline->setLogger(Logger::getLoggerPtr(AppLog));
    pipelineControl->setLogger(Logger::getLoggerPtr(AppLog));

    // initiate fetching
    pipelineControl->pulse();
}


vector<boost::shared_ptr<DataRequest>>
setupStreamMetaProcessing(FetchingParams fetchingParams, OnMetaProcessed onMetaProcessed)
{
    Name liveMetaPrefix = fetchingParams.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Live);
    Name latestPrefix = fetchingParams.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Latest);

    vector<boost::shared_ptr<DataRequest>> requests;
    {
        boost::shared_ptr<Interest> i = boost::make_shared<Interest>(liveMetaPrefix);
        i->setMustBeFresh(true);
        requests.push_back(boost::make_shared<DataRequest>(i));
    }
    {
        boost::shared_ptr<Interest> i = boost::make_shared<Interest>(latestPrefix);
        i->setMustBeFresh(true);
        requests.push_back(boost::make_shared<DataRequest>(i));
    }

    DataRequest::invokeWhenAll(requests, DataRequest::Status::Data,
                               [fetchingParams, onMetaProcessed](vector<boost::shared_ptr<DataRequest>> requests){
                                   bool firstIsMeta = (requests[0]->getNamespaceInfo().segmentClass_ == SegmentClass::Meta);
                                   auto meta = dynamic_pointer_cast<const packets::Meta>(requests[firstIsMeta ? 0 : 1]->getNdnrtcPacket());
                                   auto pointer = dynamic_pointer_cast<const packets::Pointer>(requests[firstIsMeta ? 1 : 0]->getNdnrtcPacket());
                                   uint64_t drdUsec = requests[firstIsMeta ? 0 : 1]->getDrdUsec();

                                   double samplePeriod = 1000. / meta->getLiveMeta().framerate();
                                   double ppEst = ((double)drdUsec/1000.) / samplePeriod;
                                   PacketNumber lastFrameNo = pointer->getDelegationSet().get(0).getName()[-1].toSequenceNumber();

                                   LogTrace(AppLog) << "DRD " << drdUsec/1000
                                                    << "ms, raw pp-sz est " << ppEst
                                                    << " last-seq " << lastFrameNo
                                                    << endl;

                                   int pipelineEstimate = max(3, (int)ceil(ppEst));
                                   PacketNumber nextFrame = lastFrameNo + (int)ceil((double)pipelineEstimate/2.);

                                   LogDebug(AppLog) << "pp-sz est " << pipelineEstimate << " next-seq " << nextFrame << endl;

                                   FetchingParams fp(fetchingParams);
                                   if (fp.ppAdjustable_)
                                       fp.ppSize_ = pipelineEstimate;
                                   fp.prefixInfo_.sampleNo_ = nextFrame;

                                   onMetaProcessed(fp);
                               });
    DataRequest::invokeIfAny(requests,
                             DataRequest::Status::Timeout | DataRequest::Status::NetworkNack | DataRequest::Status::AppNack,
                             [](vector<boost::shared_ptr<DataRequest>> requests){
                                 LogError(AppLog) << "received reply " << requests.back()->getStatus() << " for "
                                                  << requests.back()->getInterest()->getName() << endl;
                                 MustTerminate = true;
                             });

    return requests;
}

void printStats(boost::shared_ptr<BufferSlot> slot,
                boost::shared_ptr<v4::PipelineControl> ppCtrl,
                boost::shared_ptr<Pool<BufferSlot>> slotPool)
{
    static uint64_t startTime = 0;
    static size_t nAssembled = 0;
    static size_t nUnfetchable = 0 ;
    static uint64_t lastPacketNo = 0;
    static Average avgEsimtator(boost::make_shared<TimeWindow>(30000));
    static uint32_t outOfOrder = 0;

    if (startTime == 0)
        startTime = clock::millisecondTimestamp();

    if (slot->getState()==PipelineSlotState::Unfetchable)
        nUnfetchable++;
    else
    {
        nAssembled++;
        if (slot->getNameInfo().sampleNo_ < lastPacketNo)
            outOfOrder ++ ;
        lastPacketNo = slot->getNameInfo().sampleNo_;
    }

    avgEsimtator.newValue(slot->getLongestDrd());

    cout << "\r"
    << "[ "
    << setw(6) << setprecision(5) << (clock::millisecondTimestamp()-startTime)/1000. << "sec "
    << setw(20) << slot->getNameInfo().getSuffix(NameFilter::Sample)
    << " (" << lastPacketNo << ")"
    << " total " << nAssembled << "/" << nUnfetchable
    << " pp " << ppCtrl->getNumOutstanding() << "/" << ppCtrl->getWSize()
    << " slot-pool " << slotPool->size() << "/" << slotPool->capacity()
    << setprecision(5)
    << " asm " << setw(6) << (double)slot->getAssemblingTime()/1000.
    << " nw drd " << requestQ->getDrdEstimate()/1000.
    << " nw jttr " << requestQ->getJitterEstimate()/1000.
    << " ff-est " << avgEsimtator.value()/1000
    << " ff-jttr " << avgEsimtator.jitter()/1000
    << " lat-est " << setw(6) << (ndn_getNowMilliseconds() - slot->getFrameMeta()->getContentMetaInfo().getTimestamp())
    << " ooo " << outOfOrder
    << " ]"
    << flush;
}
