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
#include "stat.hpp"

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
void callPeriodic(uint64_t milliseconds, function<void()> callback);

typedef function<void(FetchingParams)> OnMetaProcessed;
vector<boost::shared_ptr<DataRequest>> setupStreamMetaProcessing(FetchingParams, OnMetaProcessed);

//******************************************************************************
// CONSUMER APP VARIABLES
boost::shared_ptr<Face> face;
boost::shared_ptr<helpers::KeyChainManager> keyChainManager;
boost::shared_ptr<RequestQueue> requestQ;
boost::shared_ptr<v4::PipelineControl> pipelineControl;
boost::shared_ptr<Pipeline> pipeline;
boost::shared_ptr<const packets::Meta> streamMeta;
boost::shared_ptr<const packets::Meta> liveMeta;

void runFetching(boost::asio::io_service &io,
                 std::string output,
                 const ndnrtc::NamespaceInfo& prefixInfo,
                 long ppSize,
                 long ppStep,
                 long pbcRate,
                 bool useFec,
                 bool needRvp,
                 string policyFile,
                 string csv,
                 string stats)
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

    // setup periodic _live meta fetching
    Name liveMetaPrefix = fp.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Live);
    boost::shared_ptr<Interest> i1 = boost::make_shared<Interest>(liveMetaPrefix);
    i1->setMustBeFresh(true);
    auto fetchLiveMeta = [i1](){
        auto r = boost::make_shared<DataRequest>(i1);
        r->subscribe(DataRequest::Status::Data, [](const DataRequest& r){
            liveMeta = dynamic_pointer_cast<const packets::Meta>(r.getNdnrtcPacket());
        });
        
        requestQ->enqueueRequest(r);
    };
    
    callPeriodic(5000, fetchLiveMeta);

    // setup Pipeline
    auto slotPool = boost::make_shared<Pool<BufferSlot>>(500);
    pipeline = boost::make_shared<Pipeline>(requestQ,
                                            [slotPool]()->boost::shared_ptr<IPipelineSlot>{
                                                return slotPool->pop();
                                            },
                                            fp.prefixInfo_.getPrefix(NameFilter::Stream),
                                            fp.prefixInfo_.sampleNo_, fp.ppStep_,
                                            [](const Name& framePrefix, PacketNumber seqNo){
                                                // check if nearby Key frame
                                                if (seqNo % streamMeta->getStreamMeta().gop_size() < 3)
                                                {
                                                    LogDebug(AppLog) << "generating requests for KEY " << liveMeta->getLiveMeta().segnum_key() << endl;
                                                    return Pipeline::requestsForFrame(framePrefix, seqNo,
                                                            DEFAULT_LIFETIME,
                                                            liveMeta->getLiveMeta().segnum_key(),
                                                            liveMeta->getLiveMeta().segnum_key_parity());
                                                }
                                                else
                                                {
                                                    LogDebug(AppLog) << "generating requests for DELTA " << liveMeta->getLiveMeta().segnum_delta() << endl;
                                                    return Pipeline::requestsForFrame(framePrefix, seqNo,
                                                            DEFAULT_LIFETIME,
                                                            liveMeta->getLiveMeta().segnum_delta(),
                                                            liveMeta->getLiveMeta().segnum_delta_parity());
                                                }
                                            });

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
    Name streamMetaPrefix = fetchingParams.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Meta);

    vector<boost::shared_ptr<DataRequest>> requests;
    boost::shared_ptr<Interest> i1 = boost::make_shared<Interest>(liveMetaPrefix);
    i1->setMustBeFresh(true);
    boost::shared_ptr<Interest> i2 = boost::make_shared<Interest>(latestPrefix);
    i2->setMustBeFresh(true);
    boost::shared_ptr<Interest> i3 = boost::make_shared<Interest>(streamMetaPrefix);
    i3->setMustBeFresh(true);

    auto liveMetaRequest = boost::make_shared<DataRequest>(i1);
    auto latestRequest = boost::make_shared<DataRequest>(i2);
    auto streamMetaRequest = boost::make_shared<DataRequest>(i3);

    requests.push_back(liveMetaRequest);
    requests.push_back(latestRequest);
    requests.push_back(streamMetaRequest);

    DataRequest::invokeWhenAll(requests, DataRequest::Status::Data,
                               [fetchingParams, onMetaProcessed, liveMetaRequest, streamMetaRequest, latestRequest]
                               (vector<boost::shared_ptr<DataRequest>> requests){
                                   streamMeta = dynamic_pointer_cast<const packets::Meta>(streamMetaRequest->getNdnrtcPacket());
                                   liveMeta = dynamic_pointer_cast<const packets::Meta>(liveMetaRequest->getNdnrtcPacket());

                                   auto pointer = dynamic_pointer_cast<const packets::Pointer>(latestRequest->getNdnrtcPacket());
                                   uint64_t drdUsec = liveMetaRequest->getDrdUsec();
                                   double samplePeriod = 1000. / liveMeta->getLiveMeta().framerate();
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

    // cout << "\r"
    // << "[ "
    // << setw(6) << setprecision(5) << (clock::millisecondTimestamp()-startTime)/1000. << "sec "
    // << setw(20) << slot->getNameInfo().getSuffix(NameFilter::Sample)
    // << " (" << lastPacketNo << ")"
    // << " total " << nAssembled << "/" << nUnfetchable
    // << " pp " << ppCtrl->getNumOutstanding() << "/" << ppCtrl->getWSize()
    // << " slot-pool " << slotPool->size() << "/" << slotPool->capacity()
    // << setprecision(5)
    // << " asm " << setw(6) << (double)slot->getAssemblingTime()/1000.
    // << " nw drd " << requestQ->getDrdEstimate()/1000.
    // << " nw jttr " << requestQ->getJitterEstimate()/1000.
    // << " ff-est " << avgEsimtator.value()/1000
    // << " ff-jttr " << avgEsimtator.jitter()/1000
    // << " lat-est " << setw(6) << (ndn_getNowMilliseconds() - slot->getFrameMeta()->getContentMetaInfo().getTimestamp())
    // << " ooo " << outOfOrder
    // << " ]"
    // << flush;
    for (auto &r:slot->getRequests())
    {
        if (r == *slot->getRequests().begin())
        {
            cout << clock::millisecondTimestamp()
                << "\t" << slot->getNameInfo().sampleNo_
                << "\t" << r->getDrdUsec()
                << "\t" << slot->getFetchedDataSegmentsNum() + slot->getParitySegmentsNum() + 2
                << "\t" << slot->getFetchedBytesTotal()
                << "\t" << slot->getAssemblingTime()
                << endl;
        }
        else
            if (r->getDrdUsec() != -1)
                cout << clock::millisecondTimestamp()
                    << "\t" << slot->getNameInfo().sampleNo_
                    << "\t" << r->getDrdUsec()
                    << endl;
    }

    // collect 30 seconds worth of data
    if (nAssembled == 30 * 30)
        exit(0);
}

void callPeriodic(uint64_t milliseconds, function<void()> callback)
{
    face->callLater(milliseconds, [milliseconds,callback](){
        callback();
        callPeriodic(milliseconds, callback);
    });
}
