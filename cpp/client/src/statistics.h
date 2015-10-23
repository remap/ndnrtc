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
                            ndnrtc::NdnRtcLibrary* ndnp);
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
   ndnrtc::NdnRtcLibrary* ndnp_;
   boost::asio::deadline_timer timer_;
   streamsStatisticsFiles AllStreamsStatisticsFiles_;

   // all statistics indicator names
   const std::map<std::string, ndnrtc::new_api::statistics::Indicator> IndicatorNames =
      boost::assign::map_list_of ("Acquired frames", ndnrtc::new_api::statistics::Indicator::AcquiredNum)
      ("Acquired key frames", ndnrtc::new_api::statistics::Indicator::AcquiredKeyNum)
      ("Dropped frames",  ndnrtc::new_api::statistics::Indicator::DroppedNum)
      ("Dropped key frames",  ndnrtc::new_api::statistics::Indicator::DroppedKeyNum)
      ("Assembled frames",    ndnrtc::new_api::statistics::Indicator::AssembledNum)
      ("Assembled key frames",    ndnrtc::new_api::statistics::Indicator::AssembledKeyNum)
      ("Recovered frames",    ndnrtc::new_api::statistics::Indicator::RecoveredNum)
      ("Recovered key frames",    ndnrtc::new_api::statistics::Indicator::RecoveredKeyNum)
      ("Rescued frames", ndnrtc::new_api::statistics::Indicator::RescuedNum)
      ("Rescued key frames",  ndnrtc::new_api::statistics::Indicator::RescuedKeyNum)
      ("Incomplete frames",   ndnrtc::new_api::statistics::Indicator::IncompleteNum)
      ("Incomplete key frames",   ndnrtc::new_api::statistics::Indicator::IncompleteKeyNum)
      ("Jitter target size",  ndnrtc::new_api::statistics::Indicator::BufferTargetSize)
      ("Jitter playable size",    ndnrtc::new_api::statistics::Indicator::BufferPlayableSize)
      ("Jitter estimated size",   ndnrtc::new_api::statistics::Indicator::BufferEstimatedSize)
      ("Producer rate",   ndnrtc::new_api::statistics::Indicator::CurrentProducerFramerate)
      ("Playback #",  ndnrtc::new_api::statistics::Indicator::LastPlayedNo)
      ("Last delta #",    ndnrtc::new_api::statistics::Indicator::LastPlayedDeltaNo)
      ("Last key #",  ndnrtc::new_api::statistics::Indicator::LastPlayedKeyNo)
      ("Played frames",   ndnrtc::new_api::statistics::Indicator::PlayedNum)
      ("Played key frames",   ndnrtc::new_api::statistics::Indicator::PlayedKeyNum)
      ("Skipped (no key)",    ndnrtc::new_api::statistics::Indicator::SkippedNoKeyNum)
      ("Skipped (incomplete)",    ndnrtc::new_api::statistics::Indicator::SkippedIncompleteNum)
      ("Skipped (bad GOP)",   ndnrtc::new_api::statistics::Indicator::SkippedBadGopNum)
      ("Skipped (incomplete key)",    ndnrtc::new_api::statistics::Indicator::SkippedIncompleteKeyNum)
      ("Latency (est.)",  ndnrtc::new_api::statistics::Indicator::LatencyEstimated)
      ("Delta segments average",  ndnrtc::new_api::statistics::Indicator::SegmentsDeltaAvgNum)
      ("Key segments average",    ndnrtc::new_api::statistics::Indicator::SegmentsKeyAvgNum)
      ("Delta parity segments average",   ndnrtc::new_api::statistics::Indicator::SegmentsDeltaParityAvgNum)
      ("Key parity segments average", ndnrtc::new_api::statistics::Indicator::SegmentsKeyParityAvgNum)
      ("Retransmission frequency",    ndnrtc::new_api::statistics::Indicator::RtxFrequency)
      ("Retransmissions", ndnrtc::new_api::statistics::Indicator::RtxNum)
      ("Rebufferings",    ndnrtc::new_api::statistics::Indicator::RebufferingsNum)
      ("Requested",   ndnrtc::new_api::statistics::Indicator::RequestedNum)
      ("Requested key",   ndnrtc::new_api::statistics::Indicator::RequestedKeyNum)
      ("Lambda D",    ndnrtc::new_api::statistics::Indicator::DW)
      ("Lambda",  ndnrtc::new_api::statistics::Indicator::W)
      ("RTT'",    ndnrtc::new_api::statistics::Indicator::RttPrime)
      ("Incoming bitrate (Kbps)", ndnrtc::new_api::statistics::Indicator::InBitrateKbps)
      ("Incoming segments rate",  ndnrtc::new_api::statistics::Indicator::InRateSegments)
      ("Segments received",   ndnrtc::new_api::statistics::Indicator::SegmentsReceivedNum)
      ("Timeouts",    ndnrtc::new_api::statistics::Indicator::TimeoutsNum)
      ("Darr",    ndnrtc::new_api::statistics::Indicator::Darr)
      ("RTT estimation",  ndnrtc::new_api::statistics::Indicator::RttEstimation)
      ("Interest rate",   ndnrtc::new_api::statistics::Indicator::InterestRate)
      ("Interest queue",  ndnrtc::new_api::statistics::Indicator::QueueSize)
      ("Sent interests",  ndnrtc::new_api::statistics::Indicator::InterestsSentNum)
      ("Outgoing bitarte (Kbps)", ndnrtc::new_api::statistics::Indicator::OutBitrateKbps)
      ("Outgoing segments rate",  ndnrtc::new_api::statistics::Indicator::OutRateSegments)
      ("Published frames",    ndnrtc::new_api::statistics::Indicator::PublishedNum)
      ("Published key frames",    ndnrtc::new_api::statistics::Indicator::PublishedKeyNum)
      ("Encoding rate",   ndnrtc::new_api::statistics::Indicator::EncodingRate)
      ("Capture rate",    ndnrtc::new_api::statistics::Indicator::CaptureRate)
      ("Captured frames", ndnrtc::new_api::statistics::Indicator::CapturedNum);
};