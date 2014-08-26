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
#import "CameraCapturer.h"

#include "simple-log.h"
#include "ndnrtc-library.h"
#include "view.h"
#include "config.h"
#include "renderer-stub.h"

using namespace std;
using namespace ndnrtc;
using namespace ndnlog;

@class CapturerDelegate;

int runNdnRtcApp(int argc, char * argv[]);
void printStatus(const string &str);

static bool headlessModeOn = false;

class LibraryObserver : public INdnRtcLibraryObserver
{
public:
    void onStateChanged(const char *state, const char *args);
};

void LibraryObserver::onStateChanged(const char *state, const char *args)
{
    if (headlessModeOn)
        std::cout << state << " - " << args << endl;
    else
        printStatus(string(state)+" - "+string(args));
}

static NdnRtcLibrary *ndnrtcLib = NULL;
static LibraryObserver observer;
static RendererStub LocalRenderer;

static int MonitoredProducerIdx = 0;
static pthread_t monitorThread;
static BOOL isMonitored;
std::vector<std::string> ProducerIds;

static CapturerDelegate* capturerDelegate;
static CameraCapturer* cameraCapturer;

// This class passes parameter from main to the worker thread and back.
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
@interface CapturerDelegate : NSObject<CameraCapturerDelegate>
{
    void *externalCapturer_;
}

@end

@implementation CapturerDelegate

-(id)initWithExternalCapturer:(void*)externalCapturer
{
    if ((self = [super init]))
    {
        externalCapturer_ = externalCapturer;
    }
    
    return self;
}

-(void)cameraCapturer:(CameraCapturer*)capturer didDeliveredBGRAFrameData:(NSData*)frameData
{
    if (externalCapturer_)
//        ndnrtcLib->incomingFrame(0, (unsigned char*)[frameData bytes], [frameData length]);
        ((IExternalCapturer*)externalCapturer_)->incomingArgbFrame((unsigned char*)[frameData bytes], [frameData length]);
}

-(void)cameraCapturer:(CameraCapturer*)capturer didObtainedError:(NSError*)error
{
    printStatus(string("capturer error: ") + string([[error localizedDescription] cStringUsingEncoding:NSUTF8StringEncoding]));
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
        
        result = [worker result];
    }
    return result;
}

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

int start(string username = "")
{
    ParamsStruct p, ap;
    ndnrtcLib->currentParams(p, ap);
    
    string ndnhub = "";
    
    if (username == "")
    {
        username = getInput("please, provide your username: ");
        ndnhub = getInput("please, provide hub prefix [%s]: ", p.ndnHub);
    }
    
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
    /*
    IExternalCapturer *capturer;
    ndnrtcLib->initPublishing(username.c_str(), &capturer);
    
    capturerDelegate = [[CapturerDelegate alloc] initWithExternalCapturer: capturer];
    cameraCapturer = [[CameraCapturer alloc] init];
    
    cameraCapturer.delegate = capturerDelegate;
    [cameraCapturer startCapturing];
    [cameraCapturer selectDeviceWithId:0];
    [cameraCapturer selectDeviceConfigurationWithIdx: 74];
     */
    
    
#ifdef SHOW_STATISTICS
    isMonitored = TRUE;
#endif
    return 0;
}

int join(string username = "")
{
    ParamsStruct p, ap;
    ndnrtcLib->currentParams(p, ap);
    
    string ndnhub = "";
    
    if (username == "")
    {
        username = getInput("please, provide a producer's username: ");
        ndnhub = getInput("please, provide hub prefix [%s]: ", p.ndnHub);
    }
    
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

void signalHandler(int signal)
{
    observer.onStateChanged("info", "received signal. shutting down.");
    SignalReceived = true;
}

void runHeadless(ParamsStruct &params)
{
    switch (params.headlessMode) {
        case 1: // consumer
        {
            std::cout << "info - headless mode: fetching from "
            << string(params.producerId) << endl;
            
            join(string(params.producerId));
        }
            break;
        case 2: // producer
        {
            std::cout << "info - headless mode: publishing for "
            << string(params.producerId) << endl;
            
            start(string(params.producerId));
        }
            break;
        default:
            break;
    }
    
    while (!SignalReceived) ;
}

int runNdnRtcApp(int argc, char * argv[])
{
    const char *libPath = "libndnrtc.dylib";
    const char *paramsPath = "ndnrtc.cfg";
    signal(SIGUSR1, signalHandler);
    
    if (argc >= 2)
        libPath = argv[1];
    
    if (argc >= 3)
        paramsPath = argv[2];
    
    if ((ndnrtcLib = NdnRtcLibrary::instantiateLibraryObject(libPath)))
    {
        ParamsStruct params, audioParams;
        ndnrtcLib->getDefaultParams(params, audioParams);
        ndnrtcLib->setObserver(&observer);
        
        // load default params
        if ((loadParamsFromFile(string(paramsPath), params, audioParams) == EXIT_SUCCESS))
        {
            headlessModeOn = (params.headlessMode != 0);

            if (!headlessModeOn)
                initView();
            
            ndnrtcLib->configure(params, audioParams);
        }
        
        pthread_create(&monitorThread, NULL, monitor, NULL);
        
        // check for headless mode
        if (headlessModeOn)
        {
            isMonitored = TRUE;
            runHeadless(params);
            isMonitored = NO;
        } // headless mode
        else
        {
            char input;
            
            do {
                input = plotmenu();
                
                switch (input) {
                    case 1:
                        start();
                        break;
                    case 2:
                        cameraCapturer.delegate = nil;                        
                        [cameraCapturer stopCapturing];
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
                        MonitoredProducerIdx = 0;
                        ProducerIds.push_back(testuser);
                        isMonitored = TRUE;
                    }
                        break;
                    case 7: // stat on/off
                    {
                        toggleStat();
                    }
                        break;
                    case 8:  // arrange windows
                    {
                        ndnrtcLib->arrangeWindows();
                        observer.onStateChanged("info", "windows arranged");
                    }
                        break;
                    case 9: // show stats for previous producer
                        showStatsForProducer(-1);
                        break;
                    case 10: // show stats for next producer
                        showStatsForProducer(1);
                        break;
                    default:
                        break;
                }
            } while (input != 0);

            isMonitored = NO;
            
            freeView();
        } // interactive mode
        
        NdnRtcLibrary::destroyLibraryObject(ndnrtcLib);
        ndnrtcLib = NULL;
    } // lib loaded
    
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
