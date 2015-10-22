//
//  statistics.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 8/21/13
//

#ifndef ndnrtc_statistics_h
#define ndnrtc_statistics_h

#include <string>
#include <map>
#include <stdexcept>
#include <iostream>
#include <iomanip>

#include <boost/shared_ptr.hpp>

namespace ndnrtc {
    
    namespace new_api {
        class ObjectStatistics {
        public:
            virtual ~ObjectStatistics(){}
        };
        
        namespace statistics {
            enum class Indicator {
                // general
                Timestamp,  // NDN-RTC timestamp when statistics were captured
                
                // consumer
                // buffer
                AcquiredNum,
                AcquiredKeyNum,
                DroppedNum,
                DroppedKeyNum,
                AssembledNum,
                AssembledKeyNum,
                RecoveredNum,
                RecoveredKeyNum,
                RescuedNum,
                RescuedKeyNum,
                IncompleteNum,
                IncompleteKeyNum,
                BufferTargetSize,
                BufferPlayableSize,
                BufferEstimatedSize,
                CurrentProducerFramerate,
                
                // playout
                LastPlayedNo,
                LastPlayedDeltaNo,
                LastPlayedKeyNo,
                PlayedNum,
                PlayedKeyNum,
                SkippedNoKeyNum,
                SkippedIncompleteNum,
                SkippedBadGopNum,
                SkippedIncompleteKeyNum,
                LatencyEstimated,
                
                // pipeliner
                SegmentsDeltaAvgNum,
                SegmentsKeyAvgNum,
                SegmentsDeltaParityAvgNum,
                SegmentsKeyParityAvgNum,
                RtxFrequency,
                RtxNum,
                RebufferingsNum,
                RequestedNum,
                RequestedKeyNum,
                DW,
                W,
                RttPrime,
                InBitrateKbps,
                InRateSegments,
                SegmentsReceivedNum,
                TimeoutsNum,
                Darr,
                
                // RTT estimator
                RttEstimation,
                
                // interest queue
                InterestRate,
                QueueSize,
                InterestsSentNum,
                
                // producer
                //media thread
                OutBitrateKbps,
                OutRateSegments,
                PublishedNum,
                PublishedKeyNum,
                // InterestRate, // borrowed from interest queue (above)
                
                // encoder
                // DroppedNum, // borrowed from buffer (above)
                EncodingRate,
                
                // capturer
                CaptureRate,
                CapturedNum
            };
            
            class StatisticsStorage {
            public:
                typedef std::map<Indicator, double> StatRepo;
                static const std::map<Indicator, std::string> IndicatorNames;
                
                static StatisticsStorage*
                createConsumerStatistics()
                { return new StatisticsStorage(StatisticsStorage::ConsumerStatRepo); }
                
                static StatisticsStorage*
                createProducerStatistics()
                { return new StatisticsStorage(StatisticsStorage::ProducerStatRepo); }
                
                StatisticsStorage(const StatisticsStorage& statisticsStorage):
                inidicatorNames_(StatisticsStorage::IndicatorNames),
                indicators_(statisticsStorage.getIndicators()){}
                ~StatisticsStorage(){}
                
                // may throw an exception if indicator is not present in the repo
                void
                updateIndicator(const statistics::Indicator& indicator,
                                const double& value) throw(std::out_of_range);
                
                StatRepo
                getIndicators() const;
                
                StatisticsStorage&
                operator=(const StatisticsStorage& other)
                {
                    indicators_ = other.getIndicators();
                    return *this;
                }
                
                double&
                operator[](const statistics::Indicator& indicator)
                { return indicators_.at(indicator); }
                
                friend std::ostream& operator<<(std::ostream& os,
                                                const StatisticsStorage& storage)
                {
                    for (auto& it:storage.indicators_)
                    {
                        os << std::fixed
                        << storage.inidicatorNames_.at(it.first) << "\t"
                        << std::setprecision(2) << it.second << std::endl;
                    }
                    
                    return os;
                }
            private:
                StatisticsStorage(const StatRepo& indicators):indicators_(indicators){}
                
                const std::map<Indicator, std::string> inidicatorNames_;
                static const StatRepo ConsumerStatRepo;
                static const StatRepo ProducerStatRepo;
                StatRepo indicators_;
            };

            class StatObject {
            public:
                StatObject(const boost::shared_ptr<StatisticsStorage>& statStorage):statStorage_(statStorage){}
                
                virtual ~StatObject(){}
                
            protected:
                boost::shared_ptr<StatisticsStorage> statStorage_;
            };
        };
    }
}

#endif
