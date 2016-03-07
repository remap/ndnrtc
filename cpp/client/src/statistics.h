//
//  statistics.h
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//

#include <algorithm>
#include <iterator>
#include <fstream>
#include <vector>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/date_time.hpp>
#include <boost/assign.hpp>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/statistics.h>
#include <ndnrtc/ndnrtc-library.h>

#include "config.h"

#if 0
class streamStatisticsFiles {
public:
   std::vector<std::string> streamStatisticsFilesName_;
   std::vector<std::ofstream*> streamStatisticsFilesPointer_;
};

class streamsStatisticsFiles {
public:
   std::vector<streamStatisticsFiles> streamsStatisticsFiles_;
};


class collectStreamsStatictics {
public:
   collectStreamsStatictics(boost::asio::io_service& io,
                            unsigned int headlessAppOnlineTimeSec,
                            unsigned int statisticsSampleInterval,
                            std::vector<Statistics> statistics,
                            std::vector<std::string> remoteStreamsPrefix,
                            ndnrtc::INdnRtcLibrary* ndnp);
   ~collectStreamsStatictics();
private:
   void printStatistics();
   void createStreamsStatisticsFiles();
   void closeStreamsStatisticsFiles();
   void getSingleStatistic(std::string remoteStreamsPrefix,
                           ndnrtc::new_api::statistics::StatisticsStorage remoteStreamStatistics,
                           std::string singleStatEntryName,
                           std::string &singleStatEntryContent,
                           bool getHeader) ;
   void makeRemoteStreamPrefixNicer(std::string &remoteStreamPrefixShort);


   int stoptime_ = 0;
   int statisticsSampleInterval_ = 0; // statistics sample interval in seconds
   std::vector<Statistics> statistics_;
   std::vector<std::string> remoteStreamsPrefix_;
   ndnrtc::INdnRtcLibrary* ndnp_;
   boost::asio::deadline_timer timer_;
   streamsStatisticsFiles AllStreamsStatisticsFiles_;

   // all statistics indicator names
   const std::map<std::string, ndnrtc::new_api::statistics::Indicator> IndicatorNames =
      boost::assign::map_list_of 
      ("framesAcq", ndnrtc::new_api::statistics::Indicator::AcquiredNum)
      ("framesAcqKey", ndnrtc::new_api::statistics::Indicator::AcquiredKeyNum)
      ("framesDrop",  ndnrtc::new_api::statistics::Indicator::DroppedNum)
      ("framesDropKey",  ndnrtc::new_api::statistics::Indicator::DroppedKeyNum)
      ("framesAsm",    ndnrtc::new_api::statistics::Indicator::AssembledNum)
      ("framesAsmKey",    ndnrtc::new_api::statistics::Indicator::AssembledKeyNum)
      ("framesRec",    ndnrtc::new_api::statistics::Indicator::RecoveredNum)
      ("framesRecKey",    ndnrtc::new_api::statistics::Indicator::RecoveredKeyNum)
      ("framesResc", ndnrtc::new_api::statistics::Indicator::RescuedNum)
      ("framesRescKey",  ndnrtc::new_api::statistics::Indicator::RescuedKeyNum)
      ("framesInc",   ndnrtc::new_api::statistics::Indicator::IncompleteNum)
      ("framesIncKey",   ndnrtc::new_api::statistics::Indicator::IncompleteKeyNum)
      ("jitterTar",  ndnrtc::new_api::statistics::Indicator::BufferTargetSize)
      ("jitterPlay",    ndnrtc::new_api::statistics::Indicator::BufferPlayableSize)
      ("jitterEst",   ndnrtc::new_api::statistics::Indicator::BufferEstimatedSize)
      ("prodRate",   ndnrtc::new_api::statistics::Indicator::CurrentProducerFramerate)
      ("playNo",  ndnrtc::new_api::statistics::Indicator::LastPlayedNo)
      ("deltaNo",    ndnrtc::new_api::statistics::Indicator::LastPlayedDeltaNo)
      ("keyNo",  ndnrtc::new_api::statistics::Indicator::LastPlayedKeyNo)
      ("framesPlayed",   ndnrtc::new_api::statistics::Indicator::PlayedNum)
      ("framesPlayedKey",   ndnrtc::new_api::statistics::Indicator::PlayedKeyNum)
      ("skipNoKey",    ndnrtc::new_api::statistics::Indicator::SkippedNoKeyNum)
      ("skipInc",    ndnrtc::new_api::statistics::Indicator::SkippedIncompleteNum)
      ("skipBadGop",   ndnrtc::new_api::statistics::Indicator::SkippedBadGopNum)
      ("skipIncKey",    ndnrtc::new_api::statistics::Indicator::SkippedIncompleteKeyNum)
      ("latEst",  ndnrtc::new_api::statistics::Indicator::LatencyEstimated)
      ("segAvgDelta",  ndnrtc::new_api::statistics::Indicator::SegmentsDeltaAvgNum)
      ("segAvgKey",    ndnrtc::new_api::statistics::Indicator::SegmentsKeyAvgNum)
      ("segAvgDeltaPar",   ndnrtc::new_api::statistics::Indicator::SegmentsDeltaParityAvgNum)
      ("segAvgKeyPar", ndnrtc::new_api::statistics::Indicator::SegmentsKeyParityAvgNum)
      ("rtxFreq",    ndnrtc::new_api::statistics::Indicator::RtxFrequency)
      ("rtxNum", ndnrtc::new_api::statistics::Indicator::RtxNum)
      ("rebuf",    ndnrtc::new_api::statistics::Indicator::RebufferingsNum)
      ("framesReq",   ndnrtc::new_api::statistics::Indicator::RequestedNum)
      ("framesReqKey",   ndnrtc::new_api::statistics::Indicator::RequestedKeyNum)
      ("lambdaD",    ndnrtc::new_api::statistics::Indicator::DW)
      ("lambda",  ndnrtc::new_api::statistics::Indicator::W)
      ("drdPrime",    ndnrtc::new_api::statistics::Indicator::RttPrime)
      ("bitrateIn", ndnrtc::new_api::statistics::Indicator::InBitrateKbps)
      ("segRateIn",  ndnrtc::new_api::statistics::Indicator::InRateSegments)
      ("segNum",   ndnrtc::new_api::statistics::Indicator::SegmentsReceivedNum)
      ("timeouts",    ndnrtc::new_api::statistics::Indicator::TimeoutsNum)
      ("dArr",    ndnrtc::new_api::statistics::Indicator::Darr)
      ("drdEst",  ndnrtc::new_api::statistics::Indicator::RttEstimation)
      ("irate",   ndnrtc::new_api::statistics::Indicator::InterestRate)
      ("iqueue",  ndnrtc::new_api::statistics::Indicator::QueueSize)
      ("isent",  ndnrtc::new_api::statistics::Indicator::InterestsSentNum)
      ("bitrateOut", ndnrtc::new_api::statistics::Indicator::OutBitrateKbps)
      ("segRateOut",  ndnrtc::new_api::statistics::Indicator::OutRateSegments)
      ("framesPub",    ndnrtc::new_api::statistics::Indicator::PublishedNum)
      ("framesPubKey",    ndnrtc::new_api::statistics::Indicator::PublishedKeyNum)
      ("rateEnc",   ndnrtc::new_api::statistics::Indicator::EncodingRate)
      ("rateCapture",    ndnrtc::new_api::statistics::Indicator::CaptureRate)
      ("framesCaptured", ndnrtc::new_api::statistics::Indicator::CapturedNum);
};

void callStatCollector(const unsigned int statisticsSampleInterval,
                        const unsigned int headlessAppOnlineTimeSec,
                        std::vector<Statistics> statisticsToCollect,
                        std::vector<std::string> remoteStreamsPrefix,
                        ndnrtc::INdnRtcLibrary* ndnp);
#endif