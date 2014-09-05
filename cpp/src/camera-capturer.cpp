//
//  camera-capturer.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 8/16/13
//

//#undef DEBUG

#include "camera-capturer.h"
#include "ndnrtc-object.h"
#include "ndnrtc-utils.h"

#define USE_I420

using namespace std;
using namespace ndnrtc;
using namespace webrtc;
using namespace ndnlog;

static unsigned char *frameBuffer = nullptr;

//********************************************************************************
//********************************************************************************

#pragma mark - public
//********************************************************************************
#pragma mark - construction/destruction
CameraCapturer::CameraCapturer(const ParamsStruct &params) :
BaseCapturer(params),
vcm_(nullptr)
{

}
CameraCapturer::~CameraCapturer()
{
    if (vcm_)
    {
        if (isCapturing())
            stopCapture();
        
        vcm_->Release();
    }
}

//********************************************************************************
#pragma mark - public
int CameraCapturer::init()
{
    int deviceID = params_.captureDeviceId;
    
    LogTraceC << "acquiring device " << deviceID << endl;
    
    std::vector<std::string> *availableCaptureDevices;
    availableCaptureDevices = getAvailableCaptureDevices();
    
    if (deviceID >= availableCaptureDevices->size())
    {
        printCapturingInfo();
        LogErrorC << "can't acquire device " << deviceID << std::endl;
        return notifyError(RESULT_ERR, "can't acquire device %d", deviceID);
    }
    else
        delete availableCaptureDevices;
    
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(deviceID);
    
    if (!devInfo)
        return notifyError(RESULT_ERR, "can't get device info");
    
    char deviceName [256];
    char deviceUniqueName [256];
    
    devInfo->GetDeviceName(deviceID, deviceName, 256, deviceUniqueName, 256);
    delete devInfo;
    
    LogTraceC
    << "got device name: " << deviceName
    << " (unique: " << deviceUniqueName << ")" << endl;
    
    vcm_ = VideoCaptureFactory::Create(deviceID, deviceUniqueName);
    
    if (vcm_ == NULL)
        return notifyError(RESULT_ERR, "can't get video capture module");
    
    int res = RESULT_OK;
    
    capability_.width = params_.captureWidth;
    capability_.height = params_.captureHeight;
    capability_.maxFPS = params_.captureFramerate;
    
//    kVideoI420     
//    kVideoYV12     
//    kVideoYUY2     
//    kVideoUYVY     
//    kVideoIYUV     
//    kVideoARGB     
//    kVideoRGB24    
//    kVideoRGB565   
//    kVideoARGB4444 
//    kVideoARGB1555 
//    kVideoMJPEG    
//    kVideoNV12     
//    kVideoNV21     
//    kVideoBGRA     
//    kVideoUnknown
    
    capability_.rawType = webrtc::kVideoI420; //webrtc::kVideoUnknown;
    capability_.interlaced = false;
    
    vcm_->RegisterCaptureDataCallback(*this);
    
    meterId_ = NdnRtcUtils::setupFrequencyMeter();
    
    LogInfoC << "initialized (device: " << deviceUniqueName << ")" << endl;
    
    return 0;
}
int CameraCapturer::startCapture()
{
    if (RESULT_FAIL(BaseCapturer::startCapture()))
        return RESULT_ERR;
    
    if (vcm_->StartCapture(capability_) < 0)
        return notifyError(RESULT_ERR, "capture failed to start");
    
    if (!vcm_->CaptureStarted())
        return notifyError(RESULT_ERR, "capture failed to start");
    
    LogInfoC << "started" << endl;
    isCapturing_ = true;
    
    return 0;
}
int CameraCapturer::stopCapture()
{
    BaseCapturer::stopCapture();
    
    vcm_->DeRegisterCaptureDataCallback();
    vcm_->StopCapture();
    
    LogInfoC << "stopped" << endl;
    
    return 0;
}
int CameraCapturer::numberOfCaptureDevices()
{
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
        return notifyError(-1, "can't get deivce info");
    
    return devInfo->NumberOfDevices();
}
vector<std::string>* CameraCapturer::getAvailableCaptureDevices()
{
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
    {
        notifyError(-1, "can't get deivce info");
        return nullptr;
    }
    
    vector<std::string> *devices = new vector<std::string>();
    
    static char deviceName[256];
    static char uniqueId[256];
    int numberOfDevices = numberOfCaptureDevices();
    
    for (int deviceIdx = 0; deviceIdx < numberOfDevices; deviceIdx++)
    {
        memset(deviceName, 0, 256);
        memset(uniqueId, 0, 256);
        
        if (devInfo->GetDeviceName(deviceIdx, deviceName, 256, uniqueId, 256) < 0)
        {
            notifyError(-1, "can't get info for deivce %d", deviceIdx);
            break;
        }
        
        devices->push_back(deviceName);
    }
    
    return devices;
}
void CameraCapturer::printCapturingInfo()
{
    LogInfoC << "*** Capturing info: " << endl;
    LogInfoC << "\tNumber of capture devices: " << numberOfCaptureDevices() << endl;
    LogInfoC << "\tCapture devices: " << endl;
    
    vector<std::string> *devices = getAvailableCaptureDevices();
    
    if (devices)
    {
        vector<std::string>::iterator it;
        int idx = 0;
        
        for (it = devices->begin(); it != devices->end(); ++it)
        {
            LogInfoC << "\t\t"<< idx++ << ". " << *it << endl;
        }
        delete devices;
    }
    else
        LogInfoC << "\t\t <no capture devices>" << endl;
}

//********************************************************************************
#pragma mark - overriden - webrtc::VideoCaptureDataCallback
void CameraCapturer::OnIncomingCapturedFrame(const int32_t id,
                                             I420VideoFrame& videoFrame)
{
    deliverCapturedFrame(videoFrame);
}

void CameraCapturer::OnCaptureDelayChanged(const int32_t id, const int32_t delay)
{
    LogWarnC << "delay changed: " << delay << endl;
}

//********************************************************************************
#pragma mark - private
