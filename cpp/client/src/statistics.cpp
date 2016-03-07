//
//  statistics.cpp
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//

#include "statistics.h"

using namespace std;
using namespace boost::assign;
using namespace boost::posix_time;
using namespace ndnlog;
using namespace ndnrtc;
using namespace ndnrtc::new_api;
using namespace ndnrtc::new_api::statistics;
#if 0
collectStreamsStatictics::collectStreamsStatictics(boost::asio::io_service &io,
                                                    unsigned int headlessAppOnlineTimeSec,
                                                    unsigned int statisticsSampleInterval,
                                                    std::vector<Statistics> statistics,
                                                    std::vector<std::string> remoteStreamsPrefix,
                                                    INdnRtcLibrary *ndnp)
                                                    : timer_(io, boost::posix_time::seconds(statisticsSampleInterval)),
                                                      statisticsSampleInterval_(statisticsSampleInterval),
                                                      ndnp_(ndnp),
                                                      remoteStreamsPrefix_(remoteStreamsPrefix),
                                                      statistics_ (statistics){
    stoptime_ = headlessAppOnlineTimeSec * 1000; //count in milliseconds
    
    createStreamsStatisticsFiles(); // create all statistics files and write statistics header
    timer_.async_wait(boost::bind(&collectStreamsStatictics::printStatistics, this)); // start statistics sample thread

}

collectStreamsStatictics::~collectStreamsStatictics() {
    closeStreamsStatisticsFiles();// close all statistics files
}

void collectStreamsStatictics::printStatistics() { 
    if (stoptime_ > 0) {

        int remoteStreamsPrefixNumber = remoteStreamsPrefix_.size();
        bool getHeader = false; //no statistics header, get statistics data only

        for (int remoteStreamsPrefixCount = 0; remoteStreamsPrefixCount < remoteStreamsPrefixNumber; remoteStreamsPrefixCount++) {

            StatisticsStorage remoteStreamStatistics = ndnp_->getRemoteStreamStatistics(remoteStreamsPrefix_[remoteStreamsPrefixCount]);
            int singleStreamStatisticsFileNumber = statistics_.size();

            for (int singleStreamStatisticsFileCount = 0; singleStreamStatisticsFileCount < singleStreamStatisticsFileNumber; ++singleStreamStatisticsFileCount) {

                int singleStreamStatisticsEntryNumber = statistics_[singleStreamStatisticsFileCount].gatheredStatistcs_.size();
                string remoteStreamPrefix = remoteStreamsPrefix_[remoteStreamsPrefixCount];
                string statisticsFileName = AllStreamsStatisticsFiles_.streamsStatisticsFiles_[remoteStreamsPrefixCount].streamStatisticsFilesName_[singleStreamStatisticsFileCount];
                string singleStatEntryContent = "";

                // LogDebug("") << "stream statistics file name: " << statisticsFileName << std::endl;

                getSingleStatistic(remoteStreamPrefix, remoteStreamStatistics, "Timestamp", singleStatEntryContent, getHeader);// get timestamp 

                // get every statistics entry in the for loop
                for (int singleStreamStatisticsEntryCount = 0; singleStreamStatisticsEntryCount < singleStreamStatisticsEntryNumber; ++singleStreamStatisticsEntryCount) {

                    string singleStatEntryName = statistics_[singleStreamStatisticsFileCount].gatheredStatistcs_[singleStreamStatisticsEntryCount];

                    getSingleStatistic(remoteStreamPrefix, remoteStreamStatistics, singleStatEntryName, singleStatEntryContent, getHeader);
                }
                singleStatEntryContent.append("\n");
                // write this statistics entry of a single media stream into file
                (*AllStreamsStatisticsFiles_.streamsStatisticsFiles_[remoteStreamsPrefixCount].streamStatisticsFilesPointer_[singleStreamStatisticsFileCount]) << singleStatEntryContent;
            }

        }
        stoptime_ = stoptime_ - statisticsSampleInterval_ * 1000;
        // LogDebug("") << "stoptime: " <<  stoptime_ << std::endl;
        timer_.expires_at(timer_.expires_at() + boost::posix_time::seconds(statisticsSampleInterval_));
        timer_.async_wait(boost::bind(&collectStreamsStatictics::printStatistics, this));
    }


}

void collectStreamsStatictics::createStreamsStatisticsFiles() { 
    int remoteStreamsPrefixNumber = remoteStreamsPrefix_.size();
    for (int remoteStreamsPrefixCount = 0; remoteStreamsPrefixCount < remoteStreamsPrefixNumber; remoteStreamsPrefixCount++) {

        int singleStreamStatisticsFileNumber = statistics_.size();

        // LogDebug("") << "singleStreamStatisticsFileNumber: " << singleStreamStatisticsFileNumber << std::endl;
        
        string remoteStreamPrefix = remoteStreamsPrefix_[remoteStreamsPrefixCount];
        string remoteStreamPrefixShort = remoteStreamPrefix;
        streamStatisticsFiles singleStreamStatisticsFiles;
        StatisticsStorage remoteStreamStatistics = ndnp_->getRemoteStreamStatistics(remoteStreamsPrefix_[remoteStreamsPrefixCount]);

        makeRemoteStreamPrefixNicer(remoteStreamPrefixShort);

        for (int singleStreamStatisticsFileCount = 0; singleStreamStatisticsFileCount < singleStreamStatisticsFileNumber; ++singleStreamStatisticsFileCount) {

            string statisticFileNamePrefix = statistics_[singleStreamStatisticsFileCount].statFileName_;

            // LogDebug("") << "statisticFileNamePrefix: " << statisticFileNamePrefix << std::endl;

            string statisticFileName = "";

            ofstream *statisticFilePointer = new ofstream;

            statisticFileName.append(statisticFileNamePrefix);

            statisticFileName.append(remoteStreamPrefixShort);
            statisticFileName.append(".stat");
            statisticFilePointer->open(statisticFileName.c_str());
            singleStreamStatisticsFiles.streamStatisticsFilesName_.push_back(statisticFileName);
            singleStreamStatisticsFiles.streamStatisticsFilesPointer_.push_back(statisticFilePointer);
            // LogDebug("") << "stream: " << remoteStreamPrefix << "statistics file creates:" << statisticFileName << std::endl;

            int singleStreamStatisticsEntryNumber = statistics_[singleStreamStatisticsFileCount].gatheredStatistcs_.size();
            string singleStatEntryContent = "";
            bool getHeader = true; //write statistic header to file

            getSingleStatistic(remoteStreamPrefix, remoteStreamStatistics, "Timestamp", singleStatEntryContent, getHeader);// get timestamp header
            for (int singleStreamStatisticsEntryCount = 0; singleStreamStatisticsEntryCount < singleStreamStatisticsEntryNumber; ++singleStreamStatisticsEntryCount) {

                string singleStatEntryName = statistics_[singleStreamStatisticsFileCount].gatheredStatistcs_[singleStreamStatisticsEntryCount];

                getSingleStatistic(remoteStreamPrefix, remoteStreamStatistics, singleStatEntryName, singleStatEntryContent, getHeader);
            }
            singleStatEntryContent.append("\n");
            (*statisticFilePointer) << singleStatEntryContent; //write statistics header
        }
        AllStreamsStatisticsFiles_.streamsStatisticsFiles_.push_back(singleStreamStatisticsFiles);
    }
}

void collectStreamsStatictics::closeStreamsStatisticsFiles() { 

    int remoteStreamsPrefixNumber = remoteStreamsPrefix_.size();

    for (int remoteStreamsPrefixCount = 0; remoteStreamsPrefixCount < remoteStreamsPrefixNumber; remoteStreamsPrefixCount++) {

        int singleStreamStatisticsFileNumber = statistics_.size();

        for (int singleStreamStatisticsFileCount = 0; singleStreamStatisticsFileCount < singleStreamStatisticsFileNumber; ++singleStreamStatisticsFileCount) {
            // close every file and free memory of file stream objects
            string statisticsFileName = AllStreamsStatisticsFiles_.streamsStatisticsFiles_[remoteStreamsPrefixCount].streamStatisticsFilesName_[singleStreamStatisticsFileCount];

            AllStreamsStatisticsFiles_.streamsStatisticsFiles_[remoteStreamsPrefixCount].streamStatisticsFilesPointer_[singleStreamStatisticsFileCount]->close();
            delete AllStreamsStatisticsFiles_.streamsStatisticsFiles_[remoteStreamsPrefixCount].streamStatisticsFilesPointer_[singleStreamStatisticsFileCount];
            // LogDebug("") << "stream statistics file: " << statisticsFileName << " closed" << std::endl;
        }
    }
}

void collectStreamsStatictics::getSingleStatistic(std::string remoteStreamPrefix,
                                                    StatisticsStorage remoteStreamStatistics,
                                                    std::string singleStatEntryName,
                                                    std::string &singleStatEntryContent,
                                                    bool getHeader) {

    std::map<std::string, Indicator>::const_iterator IndicatorName = this->IndicatorNames.find(singleStatEntryName);
    Indicator singleStatisticIndicator = IndicatorName->second;
    string singleStatisticString = IndicatorName->first;

    if (getHeader == true) {
        singleStatEntryContent.append(singleStatisticString);
        singleStatEntryContent.append("\t");
    } else {
        std::stringstream statisticData;

        if (singleStatisticString=="Timestamp"){
            statisticData << fixed << setprecision(0) << remoteStreamStatistics[singleStatisticIndicator]; // timestamp format
        }
        else{
            statisticData << fixed << setprecision(3) << remoteStreamStatistics[singleStatisticIndicator]; // set statistics number format (set digits after .)
        }
        singleStatEntryContent.append(statisticData.str());
        singleStatEntryContent.append("\t");
        // LogDebug("") << "remoteStreamStatistics with stream prefix" << remoteStreamPrefix << ", stat data: " << statisticData.str() << std::endl;
    }
}

void collectStreamsStatictics::makeRemoteStreamPrefixNicer(std::string &remoteStreamPrefixShort) {
    replace(remoteStreamPrefixShort.begin(), remoteStreamPrefixShort.end(), '/', '_');// no slashes in file name

    // remove some substring in remote stream prefix to make it shorter and nicer
    std::vector<std::string> cutRemoteStreamPrefixShort;

    cutRemoteStreamPrefixShort.push_back("_ndn");
    cutRemoteStreamPrefixShort.push_back("_ndnrtc_user");
    cutRemoteStreamPrefixShort.push_back("_streams");

    int cutRemoteStreamPrefixShortNumber = cutRemoteStreamPrefixShort.size();

    for (int cutRemoteStreamPrefixShortCount = 0; cutRemoteStreamPrefixShortCount < cutRemoteStreamPrefixShortNumber; ++cutRemoteStreamPrefixShortCount) {
        string::size_type cutRemoteStreamPrefixShortPosition = remoteStreamPrefixShort.find(cutRemoteStreamPrefixShort[cutRemoteStreamPrefixShortCount]);

        remoteStreamPrefixShort.erase(cutRemoteStreamPrefixShortPosition, cutRemoteStreamPrefixShort[cutRemoteStreamPrefixShortCount].length());
    }
}

void callStatCollector(const unsigned int statisticsSampleInterval,
                        const unsigned int headlessAppOnlineTimeSec,
                        std::vector<Statistics> statisticsToCollect,
                        std::vector<std::string> remoteStreamsPrefix,
                        INdnRtcLibrary* ndnp){
    // collect streams statictics
    boost::asio::io_service staticticsIo;
    LogDebug("") << "statisticsSampleInterval(s): " << statisticsSampleInterval << std::endl;
    collectStreamsStatictics collectStatictics(staticticsIo,
                                                headlessAppOnlineTimeSec,
                                                statisticsSampleInterval,
                                                statisticsToCollect,
                                                remoteStreamsPrefix,
                                                ndnp);
    staticticsIo.run();
    staticticsIo.stop();
}

#endif