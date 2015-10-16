//
//  config.h
//  ndnrtc
//
//  Created by Peter Gusev on 10/10/13.
//  Copyright (c) 2013 Peter Gusev. All rights reserved.
//

#ifndef ndnrtc_config_h
#define ndnrtc_config_h

#include <string.h>
#include <vector>
#include <libconfig.h++>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/params.h>
#include <ndnrtc/ndnrtc-library.h>

class Statistics{
public:
    std::vector<std::string> statisticItem;
    std::string statisticName;
};
class MediaStreamParamsSupplement{
public:
    //media stream main params
    ndnrtc :: new_api :: MediaStreamParams mediaStream_;
    //media stream sink
    std::string mediaStreamSink_,
        threadToFetch,
        streamPrefix;
};
class ClientParams : public ndnrtc :: new_api ::AppParams{
public:

    ndnrtc :: new_api ::GeneralParams generalParams_;
    ndnrtc :: new_api ::HeadlessModeParams headlessModeParams_;
    std::vector<Statistics> statistics_;
    int headlessAppOnlineTime=0;

    // consumer general params
    ndnrtc :: new_api ::GeneralConsumerParams
    videoConsumerParams_,
    audioConsumerParams_;

    // capture devices
    std::vector<ndnrtc :: new_api ::CaptureDeviceParams*>
    audioDevices_,
    videoDevices_;
    // capture devices source
    std::vector<ndnrtc :: new_api ::CaptureDeviceParams*>
    audioDevicesSource_,
    videoDevicesSource_;

    // default media streams
    std::vector<MediaStreamParamsSupplement>
        defaultAudioStreams_,
        defaultVideoStreams_;



    void write(std::ostream& os) const{
        os
        << "-General:" << std::endl
        << generalParams_ << std::endl
        // << "-Headless mode: " << headlessModeParams_ << std::endl
        << "-Consumer:" << std::endl
        << "--Audio: " << audioConsumerParams_ << std::endl
        << "--Video: " << videoConsumerParams_ << std::endl
        << "-Capture devices:" << std::endl
        << "--Audio:" << std::endl;

    for (int i = 0; i < audioDevices_.size(); i++)
        os << i << ": " << *audioDevices_[i] << std::endl;

    os << "--Video:" << std::endl;
    for (int i = 0; i < videoDevices_.size(); i++)
        os << i << ": " << *videoDevices_[i] << std::endl;

    os << "-Producer streams:" << std::endl
    << "--Audio:" << std::endl;

    for (int i = 0; i < defaultAudioStreams_.size(); i++)
        os << i << ": " << defaultAudioStreams_[i].mediaStream_ << std::endl;

    os << "--Video:" << std::endl;
    for (int i = 0; i < defaultVideoStreams_.size(); i++)
        os << i << ": " << defaultVideoStreams_[i].mediaStream_ << std::endl;

    }
};

int loadParamsFromFile(const std::string &cfgFileName,
                       ClientParams &params);
#endif
