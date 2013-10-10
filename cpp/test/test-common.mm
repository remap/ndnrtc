//
//  test-common.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/29/13
//

#include "test-common.h"
#include <sys/time.h>
#include "simple-log.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>

int64_t millisecondTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    int64_t ticks = 1000LL*static_cast<int64_t>(tv.tv_sec)+static_cast<int64_t>(tv.tv_usec)/1000LL;
    
    return ticks;
};


//********************************************************************************
void CocoaTestEnvironment::SetUp(){
    printf("[setting up cocoa environment]\n");
    pool_ = (void*)[[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
};

void CocoaTestEnvironment::TearDown(){
    [(NSAutoreleasePool*)pool_ release];
    printf("[tear down cocoa environment]\n");
};
