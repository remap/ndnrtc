//
//  stat.cpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/4/19.
//  Copyright © 2019 Regents of the University of California
//

#include "stat.hpp"
#include <vector>
#include <unordered_set>
#include <sstream>

#define DELIM ','

using namespace std;
using namespace ndnrtc;
using namespace ndnrtc::statistics;

vector<string> split(const string& s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

void writeHeader(FILE *f, string stats)
{
    vector<string> tokens = split(stats, DELIM);
    fprintf(f, "timestamp(ms)\t");

    int idx = 0;
    for (const auto &t:tokens)
    {
        if (idx != tokens.size()-1)
            fprintf(f, "%s\t", t.c_str());
        else
            fprintf(f, "%s\n", t.c_str());
        
        idx++;
    }
}

void writeStatLine(FILE *f, string stats,
                   const StatisticsStorage& storage)
{
    static string statsLine = "";
    static vector<Indicator> keyStats;
    
    if (stats != statsLine)
    {
        keyStats.clear();
        statsLine = stats;
        
        vector<string> tokens = split(statsLine, DELIM);
        for (const auto& t:tokens)
            for (auto &it : StatisticsStorage::IndicatorKeywords)
                if (t == it.second)
                    keyStats.push_back(it.first);
    }
    
    fprintf(f, "%lld\t", (long long)storage[Indicator::Timestamp]);
    
    int idx = 0;
    for (auto &k:keyStats)
    {
        double val = 0;
        
        try {
            val = storage[k];
        }
        catch (exception& e)
        {
            cerr << "statistic " << StatisticsStorage::IndicatorKeywords.at(k)
                 << " is not provided" << endl;
        }
        
        if (idx != keyStats.size()-1)
            fprintf(f, "%.3f\t", val);
        else
            fprintf(f, "%.3f\n", val);
        
        idx++;
    }
}

void
dumpStats(std::string csvFile, std::string stats,
          const StatisticsStorage& storage)
{
    static FILE *statFile = nullptr;
    
    if (csvFile != "")
    {
        if (!statFile)
        {
            statFile = fopen(csvFile.c_str(), "w+");
            if (!statFile)
                throw runtime_error("unable to create file: "+csvFile);
            
            writeHeader(statFile, stats);
        }
        
        if (statFile)
            writeStatLine(statFile, stats, storage);
    }
    
    fflush(statFile);
}
