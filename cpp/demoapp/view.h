//
//  view.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef ndnrtc_view_h
#define ndnrtc_view_h

#define SHOW_STATISTICS
//#define EXTENDED_STAT

#include <string.h>
#include "ndnrtc-library.h"

extern bool SignalReceived;

void initView();
void freeView();

int plotmenu();
int selectFromList(const char** listItems, int listItemsSize, const char* listTitle);
void statusUpdate(const std::string &time, const std::string &text);

#ifdef SHOW_STATISTICS
void toggleStat();
void updateStat(ndnrtc::NdnLibStatistics &stat,
                const std::string &pubPrefix,
                const std::string &fetchPrefix);
#endif

std::string getInput(const std::string &hintText);
std::string getInput(const char *hintText, ...);

#endif
