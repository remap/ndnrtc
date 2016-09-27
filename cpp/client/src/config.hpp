//
//  config.h
//  ndnrtc
//
//  Created by Jiachen Wang on 15 Octboer 2015.
//  Copyright 2013-2015 Regents of the University of California
//


#ifndef ndnrtc_config_h
#define ndnrtc_config_h

#include <string.h>
#include <vector>
#include <libconfig.h++>

#include <ndnrtc/simple-log.hpp>
#include <ndnrtc/params.hpp>

class StatGatheringParams : public ndnrtc::Params {
public:
    std::string statFileName_;

    StatGatheringParams(){}
    StatGatheringParams(const StatGatheringParams& sgp):
        statFileName_(sgp.statFileName_), gatheredStatistcs_(sgp.getStats()){}
    StatGatheringParams(const std::string& filename):statFileName_(filename){}
    
    std::string getFilename() const { return statFileName_; }
    void addStat(const std::string& stat) { gatheredStatistcs_.push_back(stat); }
    void addStats(const std::vector<std::string>& stats)
    {
        gatheredStatistcs_.insert(gatheredStatistcs_.end(), 
            stats.begin(), stats.end());
    }
    std::vector<std::string> getStats() const 
        { return std::vector<std::string>(gatheredStatistcs_); }

    void write(std::ostream& os) const {
        os 
        << "stat file: " << statFileName_
        << ".stat; stats: (";

            for (int i = 0; i < gatheredStatistcs_.size(); i++)
            {
                os << gatheredStatistcs_[i]; 
                if (i != gatheredStatistcs_.size()-1) os << ", ";
            }
            os << ")";
    }

private:
    std::vector<std::string> gatheredStatistcs_;
};

class ClientMediaStreamParams : public ndnrtc::MediaStreamParams {
public:
    std::string sessionPrefix_;

    ClientMediaStreamParams(){}
    ClientMediaStreamParams(const ClientMediaStreamParams& params):
        MediaStreamParams(params), sessionPrefix_(params.sessionPrefix_){}

    void write(std::ostream& os) const {
        os << "session prefix: " << sessionPrefix_ << "; ";
        MediaStreamParams::write(os);
    }
};

class ProducerStreamParams : public ClientMediaStreamParams {
public:
    std::string source_;

    ProducerStreamParams(){}
    ProducerStreamParams(const ProducerStreamParams& params):
        ClientMediaStreamParams(params), source_(params.source_){}

    void getMaxResolution(unsigned int& width, unsigned int& height) const;

    void write(std::ostream& os) const {
        os
        << "stream source: " << source_ << "; ";
        ClientMediaStreamParams::write(os);
    }
};

class ConsumerStreamParams : public ClientMediaStreamParams {
public:
    std::string streamSink_, threadToFetch_;

    ConsumerStreamParams(){}
    ConsumerStreamParams(const ConsumerStreamParams& params):
        ClientMediaStreamParams(params), streamSink_(params.streamSink_), 
        threadToFetch_(params.threadToFetch_){}

    void write(std::ostream& os) const {
        os 
        << "stream sink: " << streamSink_ 
        << "; thread to fetch: " << threadToFetch_ << "; ";
        ClientMediaStreamParams::write(os);
    }
};

class ConsumerClientParams : public ndnrtc::Params {
    public:
        ndnrtc::GeneralConsumerParams generalAudioParams_, generalVideoParams_;
        std::vector<StatGatheringParams> statGatheringParams_;
        std::vector<ConsumerStreamParams> fetchedStreams_;

        ConsumerClientParams(){}
        ConsumerClientParams(const ConsumerClientParams& params):
            generalAudioParams_(params.generalAudioParams_),
            generalVideoParams_(params.generalVideoParams_),
            statGatheringParams_(params.statGatheringParams_),
            fetchedStreams_(params.fetchedStreams_){}

        void write(std::ostream& os) const {
            os
            << "general audio: " << generalAudioParams_ << std::endl
            << "general video: " << generalVideoParams_ << std::endl;

            if (statGatheringParams_.size())
            {
                os << "stat gathering:" << std::endl;
                for (auto statParams:statGatheringParams_)
                    os << statParams << std::endl;
            }
            
            if (fetchedStreams_.size())
            {
                os << "fetching:" << std::endl;
                for (int i = 0; i < fetchedStreams_.size(); i++)
                    os << "[" << i << ": " << fetchedStreams_[i] << "]" << std::endl;
            }
        }
};

class ProducerClientParams : public ndnrtc::Params {
public:
    std::string prefix_;
    std::vector<ProducerStreamParams> publishedStreams_;

    ProducerClientParams(){}
    ProducerClientParams(const ProducerClientParams& params):
        prefix_(params.prefix_), 
        publishedStreams_(params.publishedStreams_){}

    void write(std::ostream& os) const {
        os 
        << "prefix: " << prefix_ << ";" << std::endl;
        
        for (int i = 0; i < publishedStreams_.size(); i++)
        {
            os << "--" << i << ":" << std::endl
            << publishedStreams_[i];
        }
    }
};  

class ClientParams : public ndnrtc::Params {
public:
    ClientParams(){}
    ~ClientParams(){}

    bool isProducing() const 
        { return publishedStreamsParams_.publishedStreams_.size() > 0; }
    bool isConsuming() const 
        { return consumedStreamsParams_.fetchedStreams_.size() > 0; }
    bool isGatheringStats() const
        { return consumedStreamsParams_.statGatheringParams_.size() > 0;}
    size_t getProducedStreamsNum() const 
        { return publishedStreamsParams_.publishedStreams_.size(); }
    size_t getConsumedStreamsNum() const
        { return consumedStreamsParams_.fetchedStreams_.size(); }
    void setGeneralParameters(const ndnrtc::GeneralParams& gp) 
        { generalParams_ = gp; }
    ndnrtc::GeneralParams getGeneralParameters() const
        { return generalParams_; }
    void setConsumerParams(const ConsumerClientParams& consumerParams)
        { consumedStreamsParams_ = consumerParams; }
    ConsumerClientParams getConsumerParams() const
        { return consumedStreamsParams_; }
    void setProducerParams(const ProducerClientParams& producerParams)
        { publishedStreamsParams_ = producerParams; }
    ProducerClientParams getProducerParams() const
        { return publishedStreamsParams_; }

    void write(std::ostream& os) const
    {
        os
        << "-general:"<< std::endl
        << generalParams_ << std::endl;

        if (isConsuming())
        {
            os << "-consuming:" << std::endl
            << consumedStreamsParams_;
        }

        if (isProducing())
        {
            os << "-producing:" << std::endl
            << publishedStreamsParams_;
        }
    }

    private:
        ndnrtc::GeneralParams generalParams_;
        ProducerClientParams publishedStreamsParams_;
        ConsumerClientParams consumedStreamsParams_;
};


int loadParamsFromFile(const std::string &cfgFileName, ClientParams &params,
    const std::string& identity);
#endif

