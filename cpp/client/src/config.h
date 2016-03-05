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

#include <ndnrtc/simple-log.h>
#include <ndnrtc/params.h>
#include <ndnrtc/ndnrtc-library.h>

class Statistics : public ndnrtc::new_api::Params {
public:
    std::vector<std::string> gatheredStatistcs_;
    std::string statFileName_;
    
    void write(std::ostream& os) const {
        os 
        << "stat file: " << statFileName_ << std::endl
        << "statistics gathered: " << std::endl;

        for (int i = 0; i < gatheredStatistcs_.size(); i++)
        {
            os << i << ": " << gatheredStatistcs_[i] << std::endl;
        }
    }
};

class MediaStreamParamsSupplement : public ndnrtc::new_api::MediaStreamParams {
public:
    //media stream sink
    std::string mediaStreamSink_,
        threadToFetch_,
        streamPrefix_;


    void write(std::ostream& os) const {
        os 
        << "stream sink: " << mediaStreamSink_ << std::endl
        << "thread to fetch: " << threadToFetch_ << std::endl
        << "stream prefix: " << streamPrefix_ << std::endl;

        MediaStreamParams::write(os);
    }
};

class ClientParams : public ndnrtc::new_api::AppParams {
public:

    std::vector<Statistics> statistics_;

    void write(std::ostream& os) const {

        AppParams::write(os);

        os 
        << "-Gathered statistics: " << std::endl;

        for (int i = 0; i < statistics_.size(); i++)
            os << i << ": " << statistics_[i] << std::endl;            
    }

    MediaStreamParamsSupplement* getMediaStream(std::vector<ndnrtc::new_api::MediaStreamParams*> defaultStreams, int streamCount){
        return ((MediaStreamParamsSupplement*)defaultStreams[streamCount]);
    }
};

int loadParamsFromFile(const std::string &cfgFileName,
                       ClientParams &params);
#endif

