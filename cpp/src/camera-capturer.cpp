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

#include "camera-capturer.h"
#include <system_wrappers/interface/tick_util.h>
#include "ndnrtc-object.h"

#define USE_I420

using namespace ndnrtc;
using namespace webrtc;

static unsigned char *frameBuffer = nullptr;

//********************************************************************************
//********************************************************************************
const std::string CameraCapturerParams::ParamNameDeviceId = "deviceId";
const std::string CameraCapturerParams::ParamNameWidth = "captureWidth";
const std::string CameraCapturerParams::ParamNameHeight = "captureHeight";
const std::string CameraCapturerParams::ParamNameFPS = "fps";

#pragma mark - public
//********************************************************************************
#pragma mark - construction/destruction
CameraCapturer::CameraCapturer(const NdnParams *params) :
NdnRtcObject(params),
vcm_(nullptr),
frameConsumer_(nullptr)
{
    TRACE("cam capturer");
}
CameraCapturer::~CameraCapturer()
{
    if (vcm_)
    {
        if (vcm_->CaptureStarted())
            vcm_->StopCapture();
        
        vcm_->Release();
    }
};

//********************************************************************************
#pragma mark - public
int CameraCapturer::init()
{
    if (!hasParams())
        return notifyErrorNoParams();
    
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
        return notifyError(-1, "can't get deivce info");
    
    int deviceID;
    
    if (getParams()->getDeviceId(&deviceID) < 0)
        return notifyErrorBadArg(CameraCapturerParams::ParamNameDeviceId);
    
    char deviceName [256];
    char deviceUniqueName [256];
    
    devInfo->GetDeviceName(deviceID, deviceName, 256, deviceUniqueName, 256);
    
    TRACE("got device name: %s, unique name: %s",deviceName, deviceUniqueName);
    
    vcm_ = VideoCaptureFactory::Create(deviceID, deviceUniqueName);
    
    if (vcm_ == NULL)
        return notifyError(-1,"can't get video capture module");
    
    if (getParams()->getWidth((int*)&capability_.width) < 0)
        return notifyErrorBadArg(CameraCapturerParams::ParamNameWidth);
    
    if (getParams()->getHeight((int*)&capability_.height) < 0)
        return notifyErrorBadArg(CameraCapturerParams::ParamNameHeight);
    
    if (getParams()->getFPS((int*)&capability_.maxFPS) < 0)
        return notifyErrorBadArg(CameraCapturerParams::ParamNameFPS);

    capability_.rawType = webrtc::kVideoI420; //webrtc::kVideoUnknown;
    
    vcm_->RegisterCaptureDataCallback(*this);
    
    return 0;
}
int CameraCapturer::startCapture()
{
    if (vcm_->StartCapture(capability_) < 0)
    {
        return notifyError(-1, "capture failed to start");
    }
    
    if (!vcm_->CaptureStarted())
    {
        return notifyError(-1, "capture failed to start");
    }
    
    INFO("started camera capture");
    
    return 0;
}
int CameraCapturer::stopCapture()
{
    vcm_->StopCapture();
    
    return 0;
}
int CameraCapturer::numberOfCaptureDevices()
{
    VideoCaptureModule::DeviceInfo *devInfo = VideoCaptureFactory::CreateDeviceInfo(0);
    
    if (!devInfo)
        return notifyError(-1, "can't get deivce info");
    
    return devInfo->NumberOfDevices();
}
vector<std::string>* CameraCapturer::availableCaptureDevices()
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
    cout << "*** Capturing info: " << endl;
    cout << "\tNumber of capture devices: " << numberOfCaptureDevices() << endl;
    cout << "\tCapture devices: " << endl;
    
    vector<std::string> *devices = availableCaptureDevices();
    
    if (devices)
    {
        vector<std::string>::iterator it;
        int idx = 0;
        
        for (it = devices->begin(); it != devices->end(); ++it)
        {
            cout << "\t\t"<< idx << ". " << *it << endl;
        }
        delete devices;
    }
    else
        cout << "\t\t <no capture devices>" << endl;
}

//********************************************************************************
#pragma mark - overriden - webrtc::VideoCaptureDataCallback
void CameraCapturer::OnIncomingCapturedFrame(const int32_t id, I420VideoFrame& videoFrame)
{
//    TRACE("captured new frame %ld",videoFrame.render_time_ms());
    
    if (videoFrame.render_time_ms() >= TickTime::MillisecondTimestamp()-30 &&
        videoFrame.render_time_ms() <= TickTime::MillisecondTimestamp())
        TRACE("..delayed");
    
#ifdef USE_I420
    if (frameConsumer_)
        frameConsumer_->onDeliverFrame(videoFrame);
#else
    int bufSize = CalcBufferSize(kARGB, videoFrame.width(), videoFrame.height());
    
    if (!frameBuffer)
    {
        TRACE("creating frame buffer of size %d", bufSize);
        frameBuffer = (unsigned char*)malloc(bufSize);
    }
    
    if (ConvertFromI420(videoFrame, kARGB, 0, frameBuffer) < 0)
    {
        ERR("can't convert from I420 to RGB");
        return;
    }
    
    if (delegate_)
        delegate_->onDeliverFrame(frameBuffer, bufSize,
                                  videoFrame.width(), videoFrame.height(),
                                  TickTime::MillisecondTimestamp(), videoFrame.render_time_ms());
    else
        TRACE("..skipping");
#endif
}

void CameraCapturer::OnCaptureDelayChanged(const int32_t id, const int32_t delay)
{
    TRACE("capture delay changed: %d", delay);
}

//********************************************************************************
#pragma mark - private
//void CameraCapturer::startBackgroundCapturingThread()
//{
//#if 0
//    vcm_->StartCapture(capability_);
//    
//    if (!vcm_->CaptureStarted())
//    {
//        ERROR("capture failed to start");
//        return;
//    }
//    
//    INFO("started camera capture");
////    nsCOMPtr<nsRunnable> captureTask = new CameraCapturerTask(capability_,vcm_);
////    captureTask->Run();
//#else
//#warning clarify the neccessity of capturing thread
//    nsresult rv;
//    nsCOMPtr<nsIThreadManager> tm = do_GetService(NS_THREADMANAGER_CONTRACTID, &rv);
//    
//    if (!capturingThread_)
//    {
//        INFO("creating new capture thread");
//        rv = tm->NewThread(0, 0, getter_AddRefs(capturingThread_));
//    }
//
//    if (NS_SUCCEEDED(rv))
//    {
//        nsCOMPtr<nsRunnable> captureTask = new CameraCapturerTask(capability_,vcm_);
//        capturingThread_->Dispatch(captureTask, nsIThread::DISPATCH_NORMAL);
//    }
//    else
//        ERROR("spin thread creation failed");
//#endif
//    
//}
