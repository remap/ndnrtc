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
#include <boost/thread.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/asio.hpp>

#include <ndnrtc/statistics.hpp>
#include <ndnrtc/stream.hpp>
#include "config.hpp"
#include "precise-generator.hpp"

namespace ndnrtc {
   class IStream;
}

/**
 * Base class for statistics writer - a worker which takes statistics storage objects,
 * extracts statistics, according to provided StatGatheringParams object and writes them
 * into an output stream.
 */
class StatWriter {
   public:
      /**
       * Base class for metric formatting
       */
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

      static std::map<std::string, ndnrtc::statistics::Indicator> IndicatorLookupTable;
   
      StatWriter(const StatGatheringParams& p, std::ostream* ostream,
         const boost::shared_ptr<IMetricFormatter>& formatter);
      virtual ~StatWriter(){ flush(); }
   
      /**
       * Extracts statisics from statRepo according to stats_ objects and writes them into ostream_
       * @param statRepo Statistics storage repo
       */
      void writeStats(const ndnrtc::statistics::StatisticsStorage::StatRepo& statRepo);

      /**
       * Flushes all unwritten data into output stream
       */
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
      void populateMetrics(const ndnrtc::statistics::StatisticsStorage::StatRepo& statRepo);
      std::vector<std::string> statOrder();

      static void initLookupTable();
};

/**
 * File-based StatWriter class
 */
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

/**
 * Metric formatter for CSV format
 */
 #define CSV_SEPARATOR "\t"
class CsvFormatter : public StatWriter::IMetricFormatter
{
   public:
      CsvFormatter(const CsvFormatter& formatter):
         IMetricFormatter(formatter),separator_(formatter.getSeparator()){}
      CsvFormatter(unsigned int precision = 2, std::string separator = CSV_SEPARATOR):
         IMetricFormatter(precision),separator_(separator){}

      std::string getSeparator() const { return separator_; }
      std::string operator()(const std::map<std::string, double>& metrics,
         const std::vector<std::string>& order) const;
      std::string getHeader(const std::map<std::string, double>& metrics,
         const std::vector<std::string>& order) const;
   
   private:
      std::string separator_;
};

/**
 * Metric formatter for JSON format
 */
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

/**
 * This class is used to collect statistics metrics from multiple streams
 * at specitifed intervals into .stat files. There could be several .stat
 * files per stream. StatGatheringParams class identifies which metrics go
 * to which file. Filename is copmosed of stream name, base file name from 
 * StatGatheringParams object and information from stream prefix (to 
 * differentiate between streams with identical names but different prefixes).
 * Statistics are gathered asynchronously into a folder.
 */
class StatCollector {
public:
   StatCollector(boost::asio::io_service& io): io_(io){}
   ~StatCollector();

   /**
    * Add stream to gather metrics from
    * @param stream Pointer to a stream (local or remote)
    */
   void addStream(const boost::shared_ptr<const ndnrtc::IStream>& stream,
                  std::string path, std::vector<StatGatheringParams> stats);
   
   /**
    * Remove stream previously added
    * @param stream Pointer to a stream 
    */
   void removeStream(const boost::shared_ptr<const ndnrtc::IStream>& stream);
   
   /**
    * Remove all streams
    */
   void removeAllStreams();

   /**
    * Returns total number of streams currently monitored
    */
   size_t getStreamsNumber() { return streamStatCollectors_.size(); }

   /**
    * Returns total number of StatWriters for all monitored streams.
    * This is useful to know the total number of .stat files currently being
    * written, as each StatWriter corresponds to one .stat file.
    */
   size_t getWritersNumber();

   /**
    * Initiates collecting statistics from previously added streams.
    * @param queryIntervalMs Defines time interval (in milliseconds) between 
    *          consecutive statistics queries
    * @param path Path to a folder where to store .stat files
    * @param stats An array of StatGatheringParams which identifies which metrics
    *          to store in .stat files.
    */
   void startCollecting(unsigned int queryIntervalMs);

   /**
    * Stops statistics gathering
    */
   void stop();

private:
   /**
    * StreamStatCollector is a helper class which stores pointer to a stream
    * and creates required number of StatWriter objects according to the 
    * provided StatGatheringParams array.
    */
   class StreamStatCollector {
   public:
      StreamStatCollector(const boost::shared_ptr<const ndnrtc::IStream>& stream):
         stream_(stream){}
      ~StreamStatCollector();

      void addStatsToCollect(std::string filePath,
         const std::vector<StatGatheringParams>& statGatheringParams);
      size_t getWritersNumber() { return statWriters_.size(); }
      void flushData();
      void writeStats();
      std::string getStreamPrefix() { return stream_->getPrefix(); }

      private:
         boost::shared_ptr<const ndnrtc::IStream> stream_;
         std::vector<StatWriter*> statWriters_;

         void prepareWriters();
         std::string fullFilePath(std::string path, std::string fname, 
            std::string basePrefix, std::string stream);
   };

   boost::asio::io_service& io_;
   boost::mutex mutex_;
   boost::shared_ptr<PreciseGenerator> generator_;
   std::map<std::string, StreamStatCollector*> streamStatCollectors_;

   void flushData();
   void finalizeEntry(StreamStatCollector& colector);
   void queryStats();

   static StatWriter* newDefaultStatWriter(const StatGatheringParams& p,
      std::string fname = "");
};

#endif
