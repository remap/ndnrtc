//
//  main.mm
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <vector>

#import "AppDelegate.h"

#include "simple-log.h"
#include "ndnrtc-library.h"

#include "view.h"
#include "config.h"
#include "renderer-stub.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnlog;

static NdnRtcLibrary *ndnrtcLib = NULL;
int runNdnRtcApp(int argc, char * argv[]);
void printStatus(const string &str);

#ifdef SHOW_STATISTICS
static int MonitoredProducerIdx = 0;
static pthread_t monitorThread;
static BOOL isMonitored;
std::vector<std::string> ProducerIds;
#endif

//******************************************************************************
// This class passes parameter from main to the worked thread and back.
@interface WorkerThread : NSObject {
    int    argc_;
    char** argv_;
    int    result_;
    bool   done_;
}

- (void)setDone:(bool)done;
- (bool)done;
- (void)setArgc:(int)argc argv:(char**)argv;
- (int) result;
- (void)run;

@end

@implementation WorkerThread

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

- (void)run{
    @autoreleasepool
    {
        result_ = runNdnRtcApp(argc_, argv_);
        done_ = true;
        [[NSApplication sharedApplication] terminate:nil];
    }
    return;
}

- (int)result {
    return result_;
}

@end

//******************************************************************************
int main(int argc, char * argv[])
{
    int result = 0;
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        WorkerThread* worker = [[WorkerThread alloc] init];
        
        [worker setArgc:argc argv:argv];
        [worker setDone:false];
        
        
        [NSThread detachNewThreadSelector:@selector(run)
                                 toTarget:worker
                               withObject:nil];

        AppDelegate *appDelegate = [[AppDelegate alloc] init];
        [NSApplication sharedApplication].delegate = appDelegate;
        [[NSApplication sharedApplication] run];

#if 0
        NSRunLoop* main_run_loop = [NSRunLoop mainRunLoop];
        NSDate *loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
        bool runloop_ok = true;
        while (![worker done] && runloop_ok) {
            @autoreleasepool {
                runloop_ok = [main_run_loop runMode:NSDefaultRunLoopMode
                                         beforeDate:loop_until];
                loop_until = [NSDate dateWithTimeIntervalSinceNow:0.1];
            }
        }
#endif
        result = [worker result];
    }
    return result;
}

//******************************************************************************
//******************************************************************************
class LibraryObserver : public INdnRtcLibraryObserver
{
public:
    void onStateChanged(const char *state, const char *args);
};

void LibraryObserver::onStateChanged(const char *state, const char *args)
{
    printStatus(string(state)+" - "+string(args));
}

//******************************************************************************
//******************************************************************************
int loadParams(ParamsStruct &videoParams, ParamsStruct &audioParams)
{
    ndnrtcLib->getDefaultParams(videoParams, audioParams);
    
    // load params from log file
    string cfgFileName = "ndnrtc.cfg";
    string input;
    
    input = getInput("please, provide file name ["+cfgFileName+"]: ");
    
    if (input != "")
        cfgFileName = input;
    
    return loadParamsFromFile(cfgFileName, videoParams, audioParams);
}

static RendererStub LocalRenderer;

int start()
{
    ParamsStruct p, ap;
    ndnrtcLib->currentParams(p, ap);
    
    string username = getInput("please, provide your username: ");
    string ndnhub = getInput("please, provide hub prefix [%s]: ", p.ndnHub);
    
    if (ndnhub != "")
    {
        p.ndnHub = (char*)malloc(ndnhub.size()+1);
        ap.ndnHub = (char*)malloc(ndnhub.size()+1);
        memset((void*)p.ndnHub, 0, ndnhub.size()+1);
        memset((void*)ap.ndnHub, 0, ndnhub.size()+1);
        ndnhub.copy((char*)p.ndnHub, ndnhub.size());
        ndnhub.copy((char*)ap.ndnHub, ndnhub.size());
        ndnrtcLib->configure(p, ap);
    }
    

    ndnrtcLib->startPublishing(username.c_str());
    
#ifdef SHOW_STATISTICS
    isMonitored = TRUE;
#endif
    return 0;
}

int join()
{
    ParamsStruct p, ap;
    ndnrtcLib->currentParams(p, ap);
    
    string username = getInput("please, provide a producer's username: ");
    string ndnhub = getInput("please, provide hub prefix [%s]: ", p.ndnHub);
    
    if (ndnhub != "")
    {
        p.ndnHub = (char*)malloc(ndnhub.size()+1);
        ap.ndnHub = (char*)malloc(ndnhub.size()+1);
        memset((void*)p.ndnHub, 0, ndnhub.size()+1);
        memset((void*)ap.ndnHub, 0, ndnhub.size()+1);
        ndnhub.copy((char*)p.ndnHub, ndnhub.size());
        ndnhub.copy((char*)ap.ndnHub, ndnhub.size());
        ndnrtcLib->configure(p, ap);
    }
    
    if (ndnrtcLib->startFetching(username.c_str()) >=0)
    {
#ifdef SHOW_STATISTICS
        isMonitored = TRUE;
        ProducerIds.push_back(username);
        MonitoredProducerIdx = (int)ProducerIds.size()-1;
#endif
    }
    
    return 0;
}

int leave()
{
    string username = getInput("please, provide a producer's username: ");
#ifdef SHOW_STATISTICS
    vector<string>::iterator it;
    
    for (it = ProducerIds.begin(); it != ProducerIds.end(); ++it)
        if (*it == username)
            break;
    
    if (it != ProducerIds.end())
        ProducerIds.erase(it);

    MonitoredProducerIdx = 0;
#endif
    
    ndnrtcLib->stopFetching(username.c_str());
    return 0;
}

#ifdef SHOW_STATISTICS
static int freq = 10;
void *monitor(void *var)
{
    static char *publisherPrefix = (char*)malloc(256);
    static char *producerPrefix = (char*)malloc(256);
    
    while (ndnrtcLib)
    {
        if (isMonitored)
        {
            NdnLibStatistics stat;
            string producerId = (ProducerIds.size())?
                                    ProducerIds[MonitoredProducerIdx]:"no fetching";
            
            ndnrtcLib->getStatistics(producerId.c_str(),
                                     stat);
            memset((void*)publisherPrefix, 0, 256);
            ndnrtcLib->getPublisherPrefix((const char**)(&publisherPrefix));
            
            memset((void*)producerPrefix, 0, 256);
            if (ProducerIds.size())
                ndnrtcLib->getProducerPrefix(producerId.c_str(),
                                             (const char**)(&producerPrefix));
            else
                memcpy((void*)producerPrefix, producerId.c_str(),
                       producerId.size());
            
            updateStat(stat, string(publisherPrefix), string(producerPrefix));
        }
        
        usleep(1000000/freq); // sleep
    }
    pthread_exit(NULL);
}

void showStatsForProducer(int dir)
{
    if (dir > 0)
    {
        if (MonitoredProducerIdx == ProducerIds.size()-1)
            MonitoredProducerIdx = 0;
        else
            MonitoredProducerIdx++;
    }
    else if (dir < 0)
    {
        if (MonitoredProducerIdx == 0)
            MonitoredProducerIdx = ProducerIds.size()-1;
        else
            MonitoredProducerIdx--;
    }
}
#endif

int runNdnRtcApp(int argc, char * argv[])
{
    const char *libPath = "libndnrtc.dylib";
    const char *paramsPath = "ndnrtc.cfg";
    
    if (argc >= 2)
        libPath = argv[1];

    if (argc >= 3)
        paramsPath = argv[2];
    
    if ((ndnrtcLib = NdnRtcLibrary::instantiateLibraryObject(libPath)))
    {
        initView();
#ifdef SHOW_STATISTICS
        if (pthread_create(&monitorThread, NULL, monitor, NULL) != 0)
            printStatus("can't start statistics monitoring");
#endif
  
        ParamsStruct params, audioParams;
        
        ndnrtcLib->getDefaultParams(params, audioParams);
        
        LibraryObserver observer;
        ndnrtcLib->setObserver(&observer);
        
        // load default params
        if ((loadParamsFromFile(string(paramsPath), params, audioParams) == EXIT_SUCCESS))
            ndnrtcLib->configure(params, audioParams);
        
        char input;
        do {
            input = plotmenu();
            
            switch (input) {
                case 1:
                    start();
                    break;
                case 2:
                    ndnrtcLib->stopPublishing();
                    break;
                case 3:
                    join();
                    break;
                case 4:
                    leave();
                    break;
                case 5:
                    if (loadParams(params, audioParams) == EXIT_SUCCESS)
                        ndnrtcLib->configure(params, audioParams);
                    else
                        observer.onStateChanged("error", "couldn't load config file");
                    
                    break;
                case 6: // loopback
                {
                    string testuser("loopback");
                    ndnrtcLib->startPublishing(testuser.c_str());
                    ndnrtcLib->startFetching(testuser.c_str());
#ifdef SHOW_STATISTICS
                    MonitoredProducerIdx = 0;
                    ProducerIds.push_back(testuser);
                    isMonitored = TRUE;
#endif
                }
                    break;
                case 7: // stat on/off
                {
#ifdef SHOW_STATISTICS
                    toggleStat();
#endif
                }
                    break;
                case 8:  // arrange windows
                {
                    ndnrtcLib->arrangeWindows();
                    observer.onStateChanged("info", "windows arranged");
                }
                    break;
                case 9: // show stats for previous producer
#ifdef SHOW_STATISTICS
                    showStatsForProducer(-1);
#endif
                    break;
                case 10: // show stats for next producer
#ifdef SHOW_STATISTICS
                    showStatsForProducer(1);
#endif
                    break;
                default:
                    break;
            }
        } while (input != 0);
#ifdef SHOW_STATISTICS
        isMonitored = NO;
#endif
        NdnRtcLibrary::destroyLibraryObject(ndnrtcLib);
        ndnrtcLib = NULL;
        
        freeView();
    }

    return 1;
}

void printStatus(const string &str)
{
    struct timeval tv;
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];
    
    gettimeofday(&tv, NULL);
    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof tmbuf, "%H:%M:%S", nowtm);
    snprintf(buf, sizeof buf, "%s.%06d : ", tmbuf, tv.tv_usec);
    
    statusUpdate(string(buf), str);
}
