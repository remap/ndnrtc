<<<<<<< HEAD
#include <ndnrtc/interfaces.h>
#include <ndnrtc/simple-log.h>
=======
// #include <ndnrtc/ndnrtc-library.h>
#include <ndnrtc/interfaces.h>
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d

class SessionObserver : public ndnrtc::ISessionObserver{
public:
    SessionObserver(){
<<<<<<< HEAD
    	LogDebug("") << "SessionObserver start..." << std::endl;
=======
    	LogDebug("") << "SessionObserver " << std::endl;
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
    }
    ~SessionObserver(){}

    void onSessionStatusUpdate(const char* username, const char* sessionPrefix,
                     			ndnrtc::SessionStatus status){
<<<<<<< HEAD
    	LogDebug("") << "onSessionStatusUpdate: username: " << username 
                   << ", sessionPrefix: " << sessionPrefix 
                   << ", SessionStatus: " << status << std::endl;
=======
    	LogDebug("") << "onSessionStatusUpdate " << std::endl;
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
    }
        
    void onSessionError(const char* username, const char* sessionPrefix,
                       ndnrtc::SessionStatus status, unsigned int errorCode,
                       const char* errorMessage){
<<<<<<< HEAD
    	LogDebug("") << "onSessionError:  username: " << username 
                   << ", sessionPrefix: " << sessionPrefix 
                   << ", SessionStatus: " << status 
                   << ", errorCode: " << errorCode
                   << ", errorMessage: " << errorMessage << std::endl;
    }
        
    void onSessionInfoUpdate(const ndnrtc::new_api::SessionInfo& sessionInfo){
    	LogDebug("") << "onSessionInfoUpdate: SessionInfo: "<< sessionInfo << std::endl;
    }
};
class readerVideoFromFile{
public:
  readerVideoFromFile(boost::asio::io_service& io, 
                      ndnrtc::IExternalCapturer** localCapturer,
                      unsigned int headlessAppOnlineTimeSec);
  ~readerVideoFromFile();
  void feedVideoFrame();

  std::ifstream *videoFrameFilePointer_;
  ndnrtc::IExternalCapturer** localCapturer_;
  int videoWidth_=720;
  int videoHeight_=405;
  int videoChannel_=4;
  int interval_=33;
  double headlessAppOnlineTimeMillisec_=0;
private:
  char* videoFramesBuffer_;
  unsigned char* videoFrameBuffer_;
  boost::asio::deadline_timer timer_;
  int frameCount_=0;
  
};
void callReaderVideoFromFile(ndnrtc::IExternalCapturer** localCapturer, unsigned int headlessAppOnlineTimeSec);
=======
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
>>>>>>> 966aab0cd09209af575b31689b3a79cfd89e315d
