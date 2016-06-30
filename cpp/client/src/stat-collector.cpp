//
//  stat-collector.cpp
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//

#include <boost/make_shared.hpp>
#include <include/name-components.h>
#include <include/ndnrtc-library.h>

#include "stat-collector.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;

//******************************************************************************
std::map<std::string, Indicator> StatWriter::IndicatorLookupTable;

StatWriter::StatWriter(const StatGatheringParams& p, std::ostream* ostream,
    const boost::shared_ptr<IMetricFormatter>& formatter):
stats_(p), ostream_(ostream), formatter_(formatter), nWrites_(0)
{     
    if (StatWriter::IndicatorLookupTable.size() == 0)
        StatWriter::initLookupTable();
}

void StatWriter::writeStats(const StatisticsStorage::StatRepo& statRepo)
{
    if (nWrites_ == 0)
    {
        setupMetrics();
        writeHeader(metricsToWrite_);
    }

    populateMetrics(statRepo);
    writeMetrics(metricsToWrite_);
    flush();

    nWrites_++;
}

void StatWriter::writeHeader(const std::map<std::string, double>& metrics)
{ 
    *ostream_ << formatter_->getHeader(metrics, statOrder()); 
}

void StatWriter::writeMetrics(const std::map<std::string, double>& metrics)
{ 
    *ostream_ << (*formatter_)(metrics, statOrder()) << endl; 
}

void StatWriter::setupMetrics()
{
    metricsToWrite_["timestamp"] = 0.;

    for (auto keyword:stats_.getStats())
        metricsToWrite_[keyword] = 0.;
}

void StatWriter::populateMetrics(const StatisticsStorage::StatRepo& statRepo)
{
    metricsToWrite_["timestamp"] = statRepo.at(Indicator::Timestamp);

    for (auto keyword:stats_.getStats())
    {
        if (IndicatorLookupTable.find(keyword) != IndicatorLookupTable.end())
        {
            Indicator ind = IndicatorLookupTable[keyword];
            if (statRepo.find(ind) != statRepo.end())
                metricsToWrite_[keyword] = statRepo.at(ind);
        }
    }
}

vector<string> StatWriter::statOrder()
{
    vector<string> order = stats_.getStats();
    order.insert(order.begin(), "timestamp");
    return order;
}

void StatWriter::initLookupTable()
{
    for (auto it:StatisticsStorage::IndicatorKeywords)
        IndicatorLookupTable[it.second] = it.first;
}

//******************************************************************************
StatFileWriter::StatFileWriter(const StatGatheringParams& p, 
    const boost::shared_ptr<IMetricFormatter>& formatter, string fname):
StatWriter(p, new ofstream(), formatter)
{
    ((ofstream*)ostream_)->open(fname.c_str(), ofstream::out);
}

StatFileWriter::~StatFileWriter()
{
    delete ostream_;
    ostream_ = nullptr;
}

//******************************************************************************
string CsvFormatter::getHeader(const std::map<std::string, double>& metrics,
    const std::vector<std::string>& order) const
{
    assert(metrics.size() == order.size());

    stringstream ss;
    
    for (auto kw:order)
    {
        ss << kw;
        if (kw != order.back()) ss << separator_;
    }
    ss << endl;
    
    return ss.str();
}

string CsvFormatter::operator()(const std::map<std::string, double>& metrics,
    const std::vector<std::string>& order) const
{
    assert(metrics.size() == order.size());

    stringstream ss;
    ss << fixed << setprecision(precision_);

    for (auto kw:order)
    {
        ss << metrics.at(kw);
        if (kw != order.back()) ss << separator_;
    }

    return ss.str();
}

//******************************************************************************
string JsonFormatter::getHeader(const std::map<std::string, double>& metrics,
    const std::vector<std::string>& order) const
{
    return "";
}

string JsonFormatter::operator()(const std::map<std::string, double>& metrics,
    const std::vector<std::string>& order) const
{
    stringstream ss;

    ss << "{";
    if (!compact_) ss << endl;
    for (auto kw:order)
    {
        if (!compact_) ss << '\t';
        ss << writeKeyValue(kw, metrics.at(kw));
        if (kw != order.back()) ss << ",";
        if (!compact_) ss << endl;
    }
    ss << "}";

    return ss.str();
}

string JsonFormatter::writeKeyValue(const std::string& key, double value) const
{
    stringstream ss;
    ss << fixed << setprecision(precision_);

    if (compact_)
        ss << "\"" << key << "\":" << value;
    else
        ss << "\"" << key << "\": " << value;

    return ss.str();
}

//******************************************************************************
StatCollector::StreamStatCollector::~StreamStatCollector()
{
    for (auto entry:statWriters_)
        delete entry;
    statWriters_.clear();
}

void StatCollector::StreamStatCollector::addStatsToCollect(string filePath,
         const vector<StatGatheringParams>& statGatheringParams)
{
    for (auto p:statGatheringParams)
    {
        string ffp = fullFilePath(filePath, p.getFilename(), streamPrefix_);
        statWriters_.push_back(StatCollector::newDefaultStatWriter(p, ffp));
    }
}

void StatCollector::StreamStatCollector::flushData()
{
    for (auto w:statWriters_)
        w->flush();
}

void StatCollector::StreamStatCollector::writeStats(const StatisticsStorage::StatRepo& repo)
{
    for (auto w:statWriters_)
        w->writeStats(repo);
}

string StatCollector::StreamStatCollector::fullFilePath(string path, string fname, 
      string streamPrefix)
{
    string fpath = path + "/" + fname + "-" + 
        NameComponents::getUserName(streamPrefix) + "-" +
        NameComponents::getStreamName(streamPrefix) + ".stat";
    return fpath;
}

//******************************************************************************
StatWriter* StatCollector::newDefaultStatWriter(const StatGatheringParams& p,
      std::string fname)
{
    return new StatFileWriter(p, boost::make_shared<JsonFormatter>(), fname);
}

StatCollector::~StatCollector()
{
    removeAllStreams();
}

void StatCollector::addStream(string streamPrefix)
{
    if (streamStatCollectors_.find(streamPrefix) == streamStatCollectors_.end())
        streamStatCollectors_[streamPrefix] = new StreamStatCollector(streamPrefix);
    else
        throw runtime_error("stream has been already added for stat gathering");
}

void StatCollector::removeStream(string streamPrefix)
{
    if (streamStatCollectors_.find(streamPrefix) != streamStatCollectors_.end())
    {
        finalizeEntry(*streamStatCollectors_[streamPrefix]);
        delete streamStatCollectors_[streamPrefix];
        streamStatCollectors_.erase(streamPrefix);
    }
}

void StatCollector::removeAllStreams()
{
    for (auto collectorEntry:streamStatCollectors_)
    {
        finalizeEntry(*collectorEntry.second);
        delete collectorEntry.second;
    }

    streamStatCollectors_.clear();
}

size_t StatCollector::getWritersNumber()
{
    size_t n = 0;
    for (auto it:streamStatCollectors_)
        n += it.second->getWritersNumber();

    return n;
}

void StatCollector::startCollecting(unsigned int queryInterval, string path, 
      vector<StatGatheringParams> stats)
{
    queryInterval_ = queryInterval;

    for (auto entry:streamStatCollectors_)
        entry.second->addStatsToCollect(path, stats);

    setupTimer();
}

void StatCollector::stop()
{
    queryTimer_.cancel();
}

#pragma mark - private
void StatCollector::finalizeEntry(StreamStatCollector& entry)
{
    entry.flushData();
}

void StatCollector::queryStats()
{
    for (auto it:streamStatCollectors_)
    {
        StatisticsStorage storage = ndnp_->getRemoteStreamStatistics(it.second->getStreamPrefix());
        it.second->writeStats(storage.getIndicators());
    }

    flushData();
    setupTimer();
}

void StatCollector::flushData()
{
    for (auto& collectorEntry:streamStatCollectors_)
        collectorEntry.second->flushData();
}

void StatCollector::setupTimer()
{
    queryTimer_.expires_from_now(boost::chrono::milliseconds(queryInterval_));
    queryTimer_.async_wait([this](const boost::system::error_code& code){
        if (code != boost::asio::error::operation_aborted){
            this->queryStats();
        }
    });
}
