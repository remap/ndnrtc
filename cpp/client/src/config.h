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
#include <libconfig.h++>

#include <ndnrtc/simple-log.h>
#include <ndnrtc/ndnrtc-library.h>

int loadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::AppParams &params);
int loadVideoStreamParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::MediaStreamParams &videoStreamParams,
                       std::string &videoStreamPrefix,
                       std::string &vthreadToFetch,
                       int videoStreamCount);
int getVideoStreamsNumberFromFile(const std::string &cfgFileName);
int loadVideoStreamThreadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::VideoThreadParams* vthreadParams,
                       int videoStreamCount,
                       int videoStreamThreadCount);
int getVideoStreamThreadNumberFromFile(const std::string &cfgFileName,
						int videoStreamCount);
int loadAudioStreamParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::MediaStreamParams &audioStreamParams,
                       std::string &audioStreamPrefix,
                       std::string &athreadToFetch,
                       int audioStreamCount);
int getAudioStreamsNumberFromFile(const std::string &cfgFileName);
int loadAudioStreamThreadParamsFromFile(const std::string &cfgFileName,
                       ndnrtc::new_api::AudioThreadParams* athreadParams,
                       int audioStreamCount,
                       int audioStreamThreadCount);
int getAudioStreamThreadNumberFromFile(const std::string &cfgFileName,
                        int audioStreamCount);
int getValueIntOrDouble(const libconfig::Setting &SettingPath, std::string lookupKey, double &paramToFind);
int getValueIntOrDouble(const libconfig::Setting &SettingPath, std::string lookupKey, int &paramToFind);
#endif
