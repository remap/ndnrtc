//
//  capturer.h
//  ndnrtc
//
//  Copyright (c) 2015 Jiachen Wang. All rights reserved.
//

#include <ndnrtc/interfaces.h>
#include <ndnrtc/simple-log.h>

class SessionObserver : public ndnrtc::ISessionObserver{
public:
    SessionObserver(){
    	LogDebug("") << "SessionObserver start..." << std::endl;
    }
    ~SessionObserver(){}

    void onSessionStatusUpdate(const char* username, const char* sessionPrefix,
                     			ndnrtc::SessionStatus status){
    	LogDebug("") << "onSessionStatusUpdate: username: " << username 
                   << ", sessionPrefix: " << sessionPrefix 
                   << ", SessionStatus: " << status << std::endl;
    }
        
    void onSessionError(const char* username, const char* sessionPrefix,
                       ndnrtc::SessionStatus status, unsigned int errorCode,
                       const char* errorMessage){
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

