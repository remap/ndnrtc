//  Copyright (C) 2013 Regents of the University of California
//  Author(s): Peter Gusev <gpeetonn@gmail.com>
//  See the LICENSE file for licensing information.
//

// Created: 7/29/2013
// Last updated: 7/29/2013

#include "ndNrtcObject.h"

#include "ndnrtc-common.h"

// internal headers
#include "data-closure.h"
#include "ndnworker.h"
#include "camera-capturer.h"

#define NDN_BUFFER_SIZE 8000

using namespace std;
using namespace ndn;
using namespace ptr_lib;
using namespace ndnrtc;

NS_IMPL_ISUPPORTS1(ndNrtc, INrtc)

//********************************************************************************
#pragma mark module loading
__attribute__((constructor))
static void initializer(int argc, char** argv, char** envp) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        INFO("module loaded");
    }
}

__attribute__((destructor))
static void destructor(){
    INFO("module unloaded");
}

//********************************************************************************
#pragma mark - creation
ndNrtc::ndNrtc(): localRenderer_(NULL), remoteRenderer_(NULL), cameraCapturer_(NULL)
{
    INFO("constructor called");    
};
ndNrtc::~ndNrtc(){
    INFO("destructor called");
//    currentWorker_ = nullptr;
};

//********************************************************************************
#pragma mark - idl interface implementation
NS_IMETHODIMP ndNrtc::StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer)
{
    CameraCapturerParams params = *CameraCapturerParams::defaultParams();
    
    
//
//    cameraCapturer_ = new CameraCapturer(params);
//    localRenderer_ = new NdnRenderer();
//    remoteRenderer_ = new NdnRenderer();
//    
//    cameraCapturer_->init();
//    cameraCapturer_->setFrameConsumer(localRenderer_);

    return NS_OK;
}

NS_IMETHODIMP ndNrtc::LeaveConference(const char *conferencePrefix, INrtcObserver *observer)
{
    return NS_OK;    
}

NS_IMETHODIMP ndNrtc::JoinConference(const char *conferencePrefix, INrtcObserver *observer)
{
    return NS_OK;
}

//********************************************************************************
#pragma mark - private
int testWebRTC()
{

    
//    cameraCapturer_->startCapture();
    
//    if (canvasRenderer_)
//        cameraCapturer_->setDelegate(canvasRenderer_);
//    
//    vcm->RegisterCaptureDataCallback(*cameraCapturer_);
//    vcm->StartCapture(capability);
//    
//    if (!vcm->CaptureStarted())
//    {
//        ERROR("capture failed to start");
//        return 0;
//    }
//
//    INFO("started camera capture");
    
    return 0;
    
//    //
//    // Create a VideoEngine instance
//    //
//    webrtc::VideoEngine* ptrViE = NULL;
//    ptrViE = webrtc::VideoEngine::Create();
//    if (ptrViE == NULL)
//    {
//        ERROR("ERROR in VideoEngine::Create\n");
//        return -1;
//    }
//    videoEngine_ = ptrViE;
//    
//    //
//    // Init VideoEngine and create a channel
//    //
//    webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
//    if (ptrViEBase == NULL)
//    {
//        ERROR("ERROR in ViEBase::GetInterface\n");
//        return -1;
//    }
//    
//    int error = ptrViEBase->Init();
//    if (error == -1)
//    {
//        ERROR("ERROR in ViEBase::Init\n");
//        return -1;
//    }
//    
//    webrtc::ViERTP_RTCP* ptrViERtpRtcp = webrtc::ViERTP_RTCP::GetInterface(ptrViE);
//    if (ptrViERtpRtcp == NULL)
//    {
//        ERROR("ERROR in ViERTP_RTCP::GetInterface\n");
//        return -1;
//    }
//    
//    int videoChannel = -1;
//    error = ptrViEBase->CreateChannel(videoChannel);
//    if (error == -1)
//    {
//        ERROR("ERROR in ViEBase::CreateChannel\n");
//        return -1;
//    }
//
//    //
//    // List available capture devices, allocate and connect.
//    //
//    webrtc::ViECapture* ptrViECapture = webrtc::ViECapture::GetInterface(ptrViE);
//    if (ptrViEBase == NULL)
//    {
//        ERROR("ERROR in ViECapture::GetInterface\n");
//        return -1;
//    }
//    
//    const unsigned int KMaxDeviceNameLength = 128;
//    const unsigned int KMaxUniqueIdLength = 256;
//    char deviceName[KMaxDeviceNameLength];
//    memset(deviceName, 0, KMaxDeviceNameLength);
//    char uniqueId[KMaxUniqueIdLength];
//    memset(uniqueId, 0, KMaxUniqueIdLength);
//    
//    INFO("Available capture devices:\n");
//    int captureIdx = 0;
//    for (captureIdx = 0;
//         captureIdx < ptrViECapture->NumberOfCaptureDevices();
//         captureIdx++)
//    {
//        memset(deviceName, 0, KMaxDeviceNameLength);
//        memset(uniqueId, 0, KMaxUniqueIdLength);
//        
//        error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
//                                                KMaxDeviceNameLength, uniqueId,
//                                                KMaxUniqueIdLength);
//        if (error == -1)
//        {
//            ERROR("ERROR in ViECapture::GetCaptureDevice\n");
//            return -1;
//        }
//        INFO("\t %d. %s\n", captureIdx + 1, deviceName);
//    }
//    ERROR("\nChoose capture device: ");
//
//    captureIdx = 0; // hard-coded for now
//
//    error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
//                                            KMaxDeviceNameLength, uniqueId,
//                                            KMaxUniqueIdLength);
//    if (error == -1)
//    {
//        ERROR("ERROR in ViECapture::GetCaptureDevice\n");
//        return -1;
//    }
//    
//    int captureId = 0;
//    error = ptrViECapture->AllocateCaptureDevice(uniqueId, KMaxUniqueIdLength,
//                                                 captureId);
//    if (error == -1)
//    {
//        ERROR("ERROR in ViECapture::AllocateCaptureDevice\n");
//        return -1;
//    }
//    
//    error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
//    if (error == -1)
//    {
//        ERROR("ERROR in ViECapture::ConnectCaptureDevice\n");
//        return -1;
//    }
//    
//    error = ptrViECapture->StartCapture(captureId);
//    if (error == -1)
//    {
//        ERROR("ERROR in ViECapture::StartCapture\n");
//        return -1;
//    }
    
}
//void ndNrtc::onDataReceived(shared_ptr<Data> &data)
//{
//    static char dataBuffer[NDN_BUFFER_SIZE];
//    
//    memset(dataBuffer, 0, NDN_BUFFER_SIZE);
//    memcpy(dataBuffer, (const char*)&data->getContent()[0], data->getContent().size());
//    
//    TRACE("got content with name %s", data->getName().to_uri().c_str());
//    
////    if (dataCallback_)
////        dataCallback_->OnNewData(data->getContent().size(), dataBuffer);
//    
//    for (unsigned int i = 0; i < data->getContent().size(); ++i)
//        cout << data->getContent()[i];
//    
//    cout << endl;
//}