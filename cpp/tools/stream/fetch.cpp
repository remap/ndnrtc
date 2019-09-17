//
// fetch.cpp
//
//  Created by Peter Gusev on 26 February 2019.
//  Copyright 2013-2019 Regents of the University of California
//

#include "fetch.hpp"

#include <chrono>
#include <mutex>



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
    string fileOut_;
} FetchingParams;

void setupFetching(shared_ptr<KeyChain> keyChain,
                   FetchingParams fetchingParams);
void setupPlayback(boost::asio::io_service&, FetchingParams fetchingParams);
void printStats(shared_ptr<BufferSlot> slot,
                shared_ptr<v4::PipelineControl> ppCtrl,
                shared_ptr<Pool<BufferSlot>> slotPool);
void callPeriodic(uint64_t milliseconds, function<void()> callback);

typedef function<void(FetchingParams)> OnMetaProcessed;
vector<shared_ptr<DataRequest>> setupStreamMetaProcessing(FetchingParams, OnMetaProcessed);

//******************************************************************************
// CONSUMER APP VARIABLES
shared_ptr<Face> face;
shared_ptr<helpers::KeyChainManager> keyChainManager;
shared_ptr<RequestQueue> requestQ;
shared_ptr<v4::PipelineControl> pipelineControl;
shared_ptr<Pipeline> pipeline;
shared_ptr<DecodeQueue> decodeQ;
shared_ptr<Buffer> buffer;
FILE *fOut = nullptr;

shared_ptr<const packets::Meta> streamMeta;
shared_ptr<const packets::Meta> liveMeta;

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
    face = make_shared<ThreadsafeFace>(io);
    keyChainManager =
                make_shared<helpers::KeyChainManager>(face,
                                                             make_shared<KeyChain>(),
                                                             "/localhost/operator", "ndnrtc-consumer",
                                                             policyFile, 3600,
                                                             Logger::getLoggerPtr(AppLog));
    requestQ = make_shared<RequestQueue>(io, face.get());
    requestQ->setLogger(Logger::getLoggerPtr(AppLog));

    decodeQ = make_shared<DecodeQueue>(io);
    decodeQ->setLogger(Logger::getLoggerPtr(AppLog));

    FetchingParams params;
    params.prefixInfo_ = prefixInfo;
    params.ppSize_ = (int)ppSize;
    params.ppStep_ = (int)ppStep;
    params.pbcRate_ = (int)pbcRate;
    params.ppAdjustable_ = (ppSize == 0);
    params.useFec_ = useFec;
    params.fileOut_ = output;

    // check if need to fetch from live stream
    if (prefixInfo.hasSeqNo_ == false)
    {
        auto metaRequests = setupStreamMetaProcessing(params,
                                                      [&](FetchingParams fp){
                                                          setupFetching(keyChainManager->instanceKeyChain(), fp);
                                                          setupPlayback(io, fp);
                                                      });
        requestQ->enqueueRequests(metaRequests);
    }
    else
    {
        setupFetching(keyChainManager->instanceKeyChain(), params);
        setupPlayback(io, params);
    }

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

    if (fOut) fclose(fOut);
}

void setupFetching(shared_ptr<KeyChain> keyChain,
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
    shared_ptr<Interest> i1 = make_shared<Interest>(liveMetaPrefix);
    i1->setMustBeFresh(true);
    auto fetchLiveMeta = [i1](){
        auto r = make_shared<DataRequest>(i1);
        r->subscribe(DataRequest::Status::Data, [](const DataRequest& r){
            liveMeta = dynamic_pointer_cast<const packets::Meta>(r.getNdnrtcPacket());
        });

        requestQ->enqueueRequest(r);
    };

    callPeriodic(1000, fetchLiveMeta);

    // setup Pipeline
    auto slotPool = make_shared<Pool<BufferSlot>>(500);
    pipeline = make_shared<Pipeline>(requestQ,
                                            [slotPool]()->shared_ptr<IPipelineSlot>{
                                                return slotPool->pop();
                                            },
                                            fp.prefixInfo_.getPrefix(NameFilter::Stream),
                                            fp.prefixInfo_.sampleNo_, fp.ppStep_,
                                            [](const Name& framePrefix, PacketNumber seqNo){
                                                LogDebug(AppLog) << "generating requests " << liveMeta->getLiveMeta().segnum_estimate() << endl;
                                                return Pipeline::requestsForFrame(framePrefix, seqNo, DEFAULT_LIFETIME,
                                                                                  liveMeta->getLiveMeta().segnum_estimate(), 0); // no FEC
                                            });

    // setup PipelineControl
    pipelineControl = make_shared<v4::PipelineControl>(fp.ppSize_);

    pipelineControl->onNewRequest.connect(bind(&Pipeline::pulse, pipeline));
    pipelineControl->onSkipPulse.connect([](){
        LogDebug(AppLog) << "pipeline-control: pulse skipped" << endl;
    });

    // push all slots discarded by decodeQ back in the pool
    decodeQ->onSlotDiscard.connect(bind(&Pool<BufferSlot>::push, slotPool, _1));

    buffer = make_shared<Buffer>(requestQ, decodeQ);
    pipeline->onNewSlot.connect(bind(&Buffer::newSlot, buffer, _1));
    buffer->onSlotDiscard.connect(bind(&Pool<BufferSlot>::push, slotPool, _1));
    // onSlotReady should connect to DecodeQ
    buffer->onSlotReady.connect([slotPool](const shared_ptr<BufferSlot>& bufferSlot)
                                {
                                    // LogInfo(AppLog) << "slot "
                                    // << bufferSlot->getNameInfo().getSuffix(NameFilter::Sample)
                                    // << " (" << bufferSlot->getNameInfo().sampleNo_
                                    // << ") fetched in " << bufferSlot->getLongestDrd()/1000 << "ms"
                                    // << " assmebled in " << (double)bufferSlot->getAssemblingTime()/1000. << "ms"
                                    // << " buffer delay estimate " << buffer->getDelayEstimate()/1000 << "ms"
                                    // << endl;

                                    if (!(Logger::getLogger(AppLog).getLogLevel() < NdnLoggerDetailLevelDefault && AppLog == ""))
                                        printStats(bufferSlot, pipelineControl, slotPool);

                                    // cout << "POOL SIZE " << slotPool->size() << endl;

//                                    buffer->removeSlot(bufferSlot->getNameInfo().sampleNo_);
//                                    LogTrace(AppLog) << buffer->dump() << endl;

                                    pipelineControl->pulse();
//                                    slotPool->push(bufferSlot);
                                });
    buffer->onSlotUnfetchable.connect([slotPool](const shared_ptr<BufferSlot>& bufferSlot)
                                      {
                                          LogWarn(AppLog) << "slot unfetchable "
                                            << bufferSlot->getNameInfo().getSuffix(NameFilter::Sample)
                                            << endl;

                                          buffer->removeSlot(bufferSlot->getNameInfo().sampleNo_);
                                          pipelineControl->pulse();
                                          slotPool->push(bufferSlot);
                                      });

    buffer->setLogger(Logger::getLoggerPtr(AppLog));
    pipeline->setLogger(Logger::getLoggerPtr(AppLog));
    pipelineControl->setLogger(Logger::getLoggerPtr(AppLog));

    // initiate fetching
    pipelineControl->pulse();
}


void setupPlayback(boost::asio::io_service& io, FetchingParams fetchingParams)
{
    int fps = 30;
    // if (fetchingParams.pbcRate_ == 0)
    // {
    //     LogInfo(AppLog) << "set up playback with internal clock" << endl;
    //     // TODO
    // }
    // else
    {
        int fps = fetchingParams.pbcRate_ ? fetchingParams.pbcRate_ : 30;
        LogInfo(AppLog) << "set up playback at " << fps << "FPS" << endl;
    }

    static int minQsize = 5;
    static bool playbackRunning = false;
    auto retrieveForRender = [fetchingParams](){
        LogDebug(AppLog) << "playback pulse. decodeQ " << decodeQ->dump() << endl;

        int seekD = -1;
        if (!playbackRunning && decodeQ->getSize() > minQsize)
        {
            LogInfo(AppLog) << "starting playback" << endl;
            playbackRunning = true;
            fOut = fopen(fetchingParams.fileOut_.c_str(), "wb");
        }
        else
            seekD = decodeQ->seek(1);

        if (playbackRunning)
        {
            if (seekD == 0)
            {
                LogDebug(AppLog) << "frame is not ready" << endl;
            }
            else
            {
                const VideoCodec::Image &img = decodeQ->get();
                if (img.getDataSize())
                {
                    NamespaceInfo* ni = (NamespaceInfo*)img.getUserData();
                    LogDebug(AppLog) << "dumped frame " << ni->sampleNo_
                                     << " " << ni->getPrefix(prefix_filter::PrefixFilter::Sample) << endl;
                    img.write(fOut);

                    // pulse pipeline control to request more frames
                    // pipelineControl->pulse();
                }
                // else
                //     LogWarn(AppLog) << "frame is not decodable" << endl;
            }
        }
    };

    static shared_ptr<PreciseGenerator> playbackPulseGenerator_ = make_shared<PreciseGenerator>(io, (double)fps, retrieveForRender);
    playbackPulseGenerator_->start();
}

vector<shared_ptr<DataRequest>>
setupStreamMetaProcessing(FetchingParams fetchingParams, OnMetaProcessed onMetaProcessed)
{
    Name liveMetaPrefix = fetchingParams.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Live);
    Name latestPrefix = fetchingParams.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Latest);
    Name streamMetaPrefix = fetchingParams.prefixInfo_.getPrefix(NameFilter::Stream).append(NameComponents::Meta);

    vector<shared_ptr<DataRequest>> requests;
    shared_ptr<Interest> i1 = make_shared<Interest>(liveMetaPrefix);
    i1->setMustBeFresh(true);
    shared_ptr<Interest> i2 = make_shared<Interest>(latestPrefix);
    i2->setMustBeFresh(true);
    shared_ptr<Interest> i3 = make_shared<Interest>(streamMetaPrefix);
    i3->setMustBeFresh(true);

    auto liveMetaRequest = make_shared<DataRequest>(i1);
    auto latestRequest = make_shared<DataRequest>(i2);
    auto streamMetaRequest = make_shared<DataRequest>(i3);

    requests.push_back(liveMetaRequest);
    requests.push_back(latestRequest);
    requests.push_back(streamMetaRequest);

    DataRequest::invokeWhenAll(requests, DataRequest::Status::Data,
                               [fetchingParams, onMetaProcessed, liveMetaRequest, streamMetaRequest, latestRequest]
                               (vector<shared_ptr<DataRequest>> requests){
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
                             [](vector<shared_ptr<DataRequest>> requests){
                                 LogError(AppLog) << "received reply " << requests.back()->getStatus() << " for "
                                                  << requests.back()->getInterest()->getName() << endl;
                                 MustTerminate = true;
                             });

    return requests;
}

void printStats(shared_ptr<BufferSlot> slot,
                shared_ptr<v4::PipelineControl> ppCtrl,
                shared_ptr<Pool<BufferSlot>> slotPool)
{
    static uint64_t startTime = 0;
    static size_t nAssembled = 0;
    static size_t nUnfetchable = 0 ;
    static uint64_t lastPacketNo = 0;
    static Average avgEstimator(make_shared<TimeWindow>(30000));
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

    // avgEstimator.newValue(slot->getLongestDrd());
    avgEstimator.newValue(slot->getAssemblingTime());

#if 1
    shared_ptr<const packets::Meta> frameMeta = slot->getMetaPacket();

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
     << " ff-est " << avgEstimator.value()/1000
     << " ff-jttr " << avgEstimator.jitter()/1000
     << " lat-est " << setw(6) << (ndn_getNowMilliseconds() - frameMeta->getContentMetaInfo().getTimestamp())
     << " ooo " << outOfOrder
     << " ]"
     << flush;
#else
    shared_ptr<const packets::Meta> frameMeta = slot->getMetaPacket();
    double ooo = requestQ->getStatistics()[statistics::Indicator::OutOfOrderNum];
    for (auto &r:slot->getRequests())
    {
        if (r == *slot->getRequests().begin())
        {
            cout << r->getReplyTimestampUsec()/1000
                << "\t" << slot->getNameInfo().sampleNo_
                << "\t" << r->getDrdUsec()
                << "\t" << ooo
                << "\t" << (frameMeta->getFrameMeta().type() == FrameMeta_FrameType_Key)
                << "\t" << slot->getFetchedDataSegmentsNum() + slot->getFetchedParitySegmentsNum() + 1
                << "\t" << slot->getFetchedBytesTotal()
                << "\t" << slot->getAssemblingTime()
                << endl;
        }
        else
            if (r->getDrdUsec() != -1)
                cout << r->getReplyTimestampUsec()/1000
                    << "\t" << slot->getNameInfo().sampleNo_
                    << "\t" << r->getDrdUsec()
                    << "\t" << ooo
                    << endl;
    }

    // collect 30 seconds worth of data
    if (nAssembled == 30 * 30)
        exit(0);
#endif
}

void callPeriodic(uint64_t milliseconds, function<void()> callback)
{
    face->callLater(milliseconds, [milliseconds,callback](){
        callback();
        callPeriodic(milliseconds, callback);
    });
}
