//
//  statistics.hpp
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
    namespace statistics {
        enum class Indicator {
                // general
                Timestamp,  // NDN-RTC timestamp when statistics were captured
                
                // consumer
                // buffer
                AcquiredNum,                    // PlaybackQueue
                AcquiredKeyNum,                 // PlaybackQueue
                DroppedNum,                     // Buffer
                DroppedKeyNum,                  // Buffer
                AssembledNum,                   // Buffer
                AssembledKeyNum,                // Buffer
                RecoveredNum,                   // VideoPlayout
                RecoveredKeyNum,                // VideoPlayout
                RescuedNum,                     // VideoPlayout
                RescuedKeyNum,                  // VideoPlayout
                IncompleteNum,                  // Buffer
                IncompleteKeyNum,               // Buffer
                BufferTargetSize,               // RemoteStreamImpl
                BufferPlayableSize,             // PlaybackQueue
                BufferReservedSize,             // PlaybackQueue
                CurrentProducerFramerate,       // BufferControl
                VerifySuccess,                  // SampleValidator
                VerifyFailure,                  // SampleValidator
                LatencyControlStable,           // LatencyControl
                LatencyControlCommand,          // LatencyControl
                
                // playout
                LastPlayedNo,                   // VideoPlayout
                LastPlayedDeltaNo,              // VideoPlayout
                LastPlayedKeyNo,                // VideoPlayout
                PlayedNum,                      // VideoPlayout
                PlayedKeyNum,                   // VideoPlayout
                SkippedNum,                     // VideoPlayout
                LatencyEstimated,
                
                // pipeliner
                SegmentsDeltaAvgNum,            // SampleEstimator
                SegmentsKeyAvgNum,              // SampleEstimator
                SegmentsDeltaParityAvgNum,      // SampleEstimator
                SegmentsKeyParityAvgNum,        // SampleEstimator
                RtxNum,
                RebufferingsNum,                // PipelineControlStateMachine
                RequestedNum,                   // Pipeliner
                RequestedKeyNum,                // Pipeliner
                DW,                             // InterestControl
                W,                              // InterestControl
                SegmentsReceivedNum,            // SegmentController
                TimeoutsNum,                    // SegmentController
                NacksNum,                       // SegmentController
                AppNackNum,                     // SegmentController
                Darr,                           // LatencyControl
                BytesReceived,                  // SegmentController
                RawBytesReceived,               // SegmentController
                State,                          // PipelineControlStateMachine
                
                // DRD estimator
                DrdOriginalEstimation,          // BufferControl
                DrdCachedEstimation,            // BufferControl
                
                // interest queue
                QueueSize,                      // InterestQueue
                InterestsSentNum,               // InterestQueue
                
                // producer
                //media thread
                BytesPublished,
                RawBytesPublished,
                PublishedSegmentsNum,
                ProcessedNum,
                PublishedNum,
                PublishedKeyNum,
                InterestsReceivedNum,
                SignNum,
                
                // encoder
                // DroppedNum, // borrowed from buffer (above)
                EncodedNum,
                
                // capturer
                CapturedNum
        };
        
        class StatisticsStorage {
        public:
                typedef std::map<Indicator, double> StatRepo;
                static const std::map<Indicator, std::string> IndicatorNames;
                static const std::map<Indicator, std::string> IndicatorKeywords;
                
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
                        try {
                            os << std::fixed
                            << storage.inidicatorNames_.at(it.first) << "\t"
                            << std::setprecision(2) << it.second << std::endl;
                        }
                        catch (...) {
                        }
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

#endif
