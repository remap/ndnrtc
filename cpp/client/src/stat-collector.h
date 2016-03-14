//
//  stat-collector.h
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//

#ifndef __stat_collector_h__
#define __stat_collector_h__

#include <stdlib.h>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <vector>
#include <boost/asio/steady_timer.hpp>
#include <boost/shared_ptr.hpp>

#include "config.h"

class StatWriter {
   public:
      class IMetricFormatter {
      public:
         IMetricFormatter(const IMetricFormatter& formatter):
            precision_(formatter.getPrecision()){}
         IMetricFormatter(unsigned int precision = 2):precision_(precision){}

         unsigned int getPrecision() const { return precision_; }
         virtual std::string operator()(const std::map<std::string, double>& metrics,
            const std::vector<std::string>& order) const = 0;
         virtual std::string getHeader(const std::map<std::string, double>& metrics,
            const std::vector<std::string>& order) const = 0;
      
      protected:
         unsigned int precision_;
      };

      static std::map<std::string, ndnrtc::new_api::statistics::Indicator> IndicatorLookupTable;
   
      StatWriter(const StatGatheringParams& p, std::ostream* ostream,
         const boost::shared_ptr<IMetricFormatter>& formatter);
      virtual ~StatWriter(){ flush(); }
   
      void writeStats(const ndnrtc::new_api::statistics::StatisticsStorage::StatRepo& statRepo);
      void flush() { if (ostream_) ostream_->flush(); }
   
   protected:
      std::ostream* ostream_;

      virtual void writeHeader(const std::map<std::string, double>& metrics);
      virtual void writeMetrics(const std::map<std::string, double>& metrics);
   
   private:
      unsigned int nWrites_;
      boost::shared_ptr<IMetricFormatter> formatter_;
      StatGatheringParams stats_;
      std::map<std::string, double> metricsToWrite_;
   
      void setupMetrics();
      void populateMetrics(const ndnrtc::new_api::statistics::StatisticsStorage::StatRepo& statRepo);
      std::vector<std::string> statOrder();

      static void initLookupTable();
};

class StatFileWriter : public StatWriter {
   public:
      StatFileWriter(const StatGatheringParams& p, 
         const boost::shared_ptr<IMetricFormatter>& formatter,
       std::string fname);
      ~StatFileWriter();

   private:
      StatFileWriter(StatFileWriter const&) = delete;
      void operator=(StatFileWriter const&) = delete;
};

class CsvFormatter : public StatWriter::IMetricFormatter
{
   public:
      CsvFormatter(const CsvFormatter& formatter):
         IMetricFormatter(formatter),separator_(formatter.getSeparator()){}
      CsvFormatter(unsigned int precision = 2, std::string separator = "\t"):
         IMetricFormatter(precision),separator_(separator){}

      std::string getSeparator() const { return separator_; }
      std::string operator()(const std::map<std::string, double>& metrics,
         const std::vector<std::string>& order) const;
      std::string getHeader(const std::map<std::string, double>& metrics,
         const std::vector<std::string>& order) const;
   
   private:
      std::string separator_;
};

class JsonFormatter : public StatWriter::IMetricFormatter
{
   public:
      JsonFormatter(const JsonFormatter& formatter):
         IMetricFormatter(formatter),compact_(formatter.isCompact()){}
      JsonFormatter(unsigned int precision = 2, bool compact = true):
         IMetricFormatter(precision),compact_(compact){}

      bool isCompact() const { return compact_; }
      std::string operator()(const std::map<std::string, double>& metrics,
         const std::vector<std::string>& order) const;
      std::string getHeader(const std::map<std::string, double>& metrics,
         const std::vector<std::string>& order) const;

   private:
      bool compact_;

      std::string writeKeyValue(const std::string& key, double value) const;
};

class StatCollector {
public:
   StatCollector(boost::asio::io_service& io, ndnrtc::INdnRtcLibrary* ndnp):
      io_(io), ndnp_(ndnp), queryTimer_(io){}
   ~StatCollector();

   void addStream(std::string streamPrefix);
   void removeStream(std::string streamPrefix);
   void removeAllStreams();

   size_t getStreamsNumber() { return streamStatCollectors_.size(); }
   size_t getWritersNumber();

   void startCollecting(unsigned int queryIntervalMs, std::string path, 
      std::vector<StatGatheringParams> stats);
   void stop();

private:
   class StreamStatCollector {
   public:
      StreamStatCollector(std::string streamPrefix):
         streamPrefix_(streamPrefix){}
      ~StreamStatCollector();

      void addStatsToCollect(std::string filePath,
         const std::vector<StatGatheringParams>& statGatheringParams);
      size_t getWritersNumber() { return statWriters_.size(); }
      void flushData();
      void writeStats(const ndnrtc::new_api::statistics::StatisticsStorage::StatRepo& repo);
      std::string getStreamPrefix() { return streamPrefix_; }

      private:
         std::string streamPrefix_;
         std::vector<StatWriter*> statWriters_;

         void prepareWriters();
         std::string fullFilePath(std::string path, std::string fname, 
            std::string srteamPrefix);
   };

   ndnrtc::INdnRtcLibrary* ndnp_;
   boost::asio::io_service& io_;
   boost::asio::steady_timer queryTimer_;
   unsigned int queryInterval_;
   std::map<std::string, StreamStatCollector*> streamStatCollectors_;

   void flushData();
   void finalizeEntry(StreamStatCollector& colector);
   
   void queryStats();
   void setupTimer();

   static StatWriter* newDefaultStatWriter(const StatGatheringParams& p,
      std::string fname = "");
};

#endif