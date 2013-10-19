//
//  test-main-mac.m
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#import <Cocoa/Cocoa.h>
#import "test-common.h"
#include "simple-log.h"

int runTests(int argc, char * argv[]);

// This class passes parameter from main to the worked thread and back.
@interface AutoTestInWorkerThread : NSObject {
    int    argc_;
    char** argv_;
    int    result_;
    bool   done_;
}

- (void)setDone:(bool)done;
- (bool)done;
- (void)setArgc:(int)argc argv:(char**)argv;
- (int) result;
- (void)runTest:(NSObject*)ignored;

@end

@implementation AutoTestInWorkerThread

- (void)setDone:(bool)done {
    done_ = done;
}

- (bool)done {
    return done_;
}

- (void)setArgc:(int)argc argv:(char**)argv {
    argc_ = argc;
    argv_ = argv;
}

- (void)runTest:(NSObject*)ignored {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    result_ = runTests(argc_, argv_);
    done_ = true;
    
    [pool release];
    return;
}

- (int)result {
    return result_;
}

@end

GTEST_API_ int runTests(int argc, char * argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

int main(int argc, char * argv[]) {
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
    [NSApplication sharedApplication];
    
    int result = 0;
    AutoTestInWorkerThread* tests = [[AutoTestInWorkerThread alloc] init];
    
    [tests setArgc:argc argv:argv];
    [tests setDone:false];
    
    [NSThread detachNewThreadSelector:@selector(runTest:)
                             toTarget:tests
                           withObject:nil];
    
    NSRunLoop* main_run_loop = [NSRunLoop mainRunLoop];
    NSDate *loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
    bool runloop_ok = true;
    
    while (![tests done] && runloop_ok) {
        runloop_ok = [main_run_loop runMode:NSDefaultRunLoopMode
                                 beforeDate:loop_until];
        loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
    }
    
    result = [tests result];
    
    [pool release];
    return result;
}
