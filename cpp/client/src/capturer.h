// #include <ndnrtc/ndnrtc-library.h>
#include <ndnrtc/interfaces.h>

class SessionObserver : public ndnrtc::ISessionObserver{
public:
    SessionObserver(){
    	LogDebug("") << "SessionObserver " << std::endl;
    }
    ~SessionObserver(){}

    void onSessionStatusUpdate(const char* username, const char* sessionPrefix,
                     			ndnrtc::SessionStatus status){
    	LogDebug("") << "onSessionStatusUpdate " << std::endl;
    }
        
    void onSessionError(const char* username, const char* sessionPrefix,
                       ndnrtc::SessionStatus status, unsigned int errorCode,
                       const char* errorMessage){
    	LogDebug("") << "onSessionError " << std::endl;
    }
        
    void onSessionInfoUpdate(const ndnrtc::new_api::SessionInfo& sessionInfo){
    	LogDebug("") << "onSessionInfoUpdate " << std::endl;
    }
        // @autoreleasepool {
        //     if (![NCNdnRtcLibraryController sharedInstance].sessionPrefix)
        //         [NCNdnRtcLibraryController sharedInstance].sessionPrefix = [NSString stringWithCString:sessionPrefix encoding:NSASCIIStringEncoding];
            
        //     NCSessionStatus oldStatus = [NCNdnRtcLibraryController sharedInstance].sessionStatus;
        //     [NCNdnRtcLibraryController sharedInstance].sessionStatus = [NCNdnRtcLibraryController ncStatus:status];
            
        //     NSLog(@"new local session status - %d", status);
        //     NSString *usernameStr = [NSString ncStringFromCString:username];
        //     NSString *sessionPrefixStr = [NSString ncStringFromCString:sessionPrefix];
        //     dispatch_async(dispatch_get_main_queue(), ^{
        //         [[[NSObject alloc] init] notifyNowWithNotificationName:NCLocalSessionStatusUpdateNotification
        //                                                    andUserInfo:@{kSessionUsernameKey: usernameStr,
        //                                                                  kSessionPrefixKey: sessionPrefixStr,
        //                                                                  kSessionStatusKey: @([NCNdnRtcLibraryController ncStatus:status]),
        //                                                                  kSessionOldStatusKey: @(oldStatus)}];
        //     });
        // }
};
class ExternalCapturer: public ndnrtc::IExternalCapturer{
public:
    void capturingStarted(){
    	LogDebug("") << "capturingStarted " << std::endl;
    }
    
    void capturingStopped(){}
    
    int incomingArgbFrame(const unsigned int width, //BRGA ARGB
                                  const unsigned int height,
                                  unsigned char* argbFrameData,
                                  unsigned int frameSize){
    	LogDebug("") << "incomingArgbFrame " << std::endl;
    }
    
    int incomingI420Frame(const unsigned int width,
                                  const unsigned int height,
                                  const unsigned int strideY,
                                  const unsigned int strideU,
                                  const unsigned int strideV,
                                  const unsigned char* yBuffer,
                                  const unsigned char* uBuffer,
                                  const unsigned char* vBuffer) {
    	LogDebug("") << "incomingI420Frame " << std::endl;
    }
};