//
//  ndNrtcObject.cpp
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev 
//  Created: 7/29/13
//

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
ndNrtc::ndNrtc(): senderChannel_(nullptr)
{
    INFO("constructor called");    
};
ndNrtc::~ndNrtc(){
    INFO("destructor called");
    
    if (senderChannel_->isTransmitting())
        senderChannel_->stopTransmission();
};

//********************************************************************************
#pragma mark - public
void ndNrtc::onErrorOccurred(const char *errorMessage)
{
    TRACE("error occurred: %s", errorMessage);
}

//********************************************************************************
#pragma mark - idl interface implementation
NS_IMETHODIMP ndNrtc::StartConference(nsIPropertyBag2 *prop, INrtcObserver *observer)
{
#if 1
    int width = 352, height = 288;
    FILE *f = fopen("resources/foreman_cif.yuv", "rb");
    
    int32_t frameSize = webrtc::CalcBufferSize(webrtc::kI420, width, height);
    unsigned char* frameData = new unsigned char[frameSize];
    
    fread(frameData, 1, frameSize, f);
    
    int size_y = width * height;
    int size_uv = ((width + 1) / 2)  * ((height + 1) / 2);
    webrtc::I420VideoFrame *sampleFrame_ = new webrtc::I420VideoFrame();
    
    sampleFrame_->CreateFrame(size_y, frameData,
                              size_uv,frameData + size_y,
                              size_uv, frameData + size_y + size_uv,
                              width, height,
                              width,
                              (width + 1) / 2, (width + 1) / 2);
    
    webrtc::VideoCodec codec_;
    webrtc::VideoEncoder *encoder_;
    
    if (!webrtc::VCMCodecDataBase::Codec(VCM_VP8_IDX, &codec_))
    {
        TRACE("can't get deafult params");
        strncpy(codec_.plName, "VP8", 31);
        codec_.maxFramerate = 30;
        codec_.startBitrate  = 300;
        codec_.maxBitrate = 4000;
        codec_.width = width;
        codec_.height = height;
        codec_.plType = VCM_VP8_PAYLOAD_TYPE;
        codec_.qpMax = 56;
        codec_.codecType = webrtc::kVideoCodecVP8;
        codec_.codecSpecific.VP8.denoisingOn = true;
        codec_.codecSpecific.VP8.complexity = webrtc::kComplexityNormal;
        codec_.codecSpecific.VP8.numberOfTemporalLayers = 1;
    }
    
    encoder_ = webrtc::VP8Encoder::Create();
    
    NdnVideoCoderParams *coderParams_ = NdnVideoCoderParams::defaultParams();
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    
    
    if (!encoder_)
        return NS_OK;
    
    int maxPayload = 3900;
    
    if (encoder_->InitEncode(&codec_, 1, maxPayload) != WEBRTC_VIDEO_CODEC_OK)
        return NS_OK;
    
    encoder_->RegisterEncodeCompleteCallback(vc);
    
    int err = encoder_->Encode(*sampleFrame_, NULL, NULL);
    if (err != WEBRTC_VIDEO_CODEC_OK)
    {
        TRACE("encode error %d",err);
        return NS_OK;
    }
    else
        TRACE("encoded!!1");        
    
    return NS_OK;
#endif
    
#if 0
    NdnVideoCoderParams *coderParams_ = NdnVideoCoderParams::defaultParams();
    
    int width = 352, height = 288;
    // change frame size according to the data in resource file file
    coderParams_->setIntParam(NdnVideoCoderParams::ParamNameWidth, width);
    coderParams_->setIntParam(NdnVideoCoderParams::ParamNameHeight, height);
    
    FILE *f = fopen("resources/foreman_cif.yuv", "rb");
    
    int32_t frameSize = webrtc::CalcBufferSize(webrtc::kI420, width, height);
    unsigned char* frameData = new unsigned char[frameSize];
    
    fread(frameData, 1, frameSize, f);
    
    int size_y = width * height;
    int size_uv = ((width + 1) / 2)  * ((height + 1) / 2);
    webrtc::I420VideoFrame *sampleFrame_ = new webrtc::I420VideoFrame();
    
    sampleFrame_->CreateFrame(size_y, frameData,
                                        size_uv,frameData + size_y,
                                        size_uv, frameData + size_y + size_uv,
                                        width, height,
                                        width,
                                        (width + 1) / 2, (width + 1) / 2);
    
    fclose(f);
    delete [] frameData;

    
    NdnVideoCoder *vc = new NdnVideoCoder(coderParams_);
    
    vc->init();
    
    vc->onDeliverFrame(*sampleFrame_);
    
    return NS_OK;
#endif
    
    shared_ptr<NdnParams> params(NdnSenderChannel::defaultParams());
    shared_ptr<NdnSenderChannel> sc(new NdnSenderChannel(params.get()));

    sc->setObserver(this);
    
    if (sc->init() < 0)
        return notifyObserverError(observer, "can't initialize sender channel");

    if (sc->startTransmission() < 0)
        return notifyObserverError(observer, "can't start transmission");
    
    senderChannel_ = sc;
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
nsresult ndNrtc::notifyObserverError(INrtcObserver *obs, const char *format, ...)
{
    TRACE("notify observer error");
    
    va_list args;
    
    static char emsg[256];
    
    va_start(args, format);
    vsprintf(emsg, format, args);
    va_end(args);
    
    notifyObserver(obs, "error", emsg);

    return NS_OK;
}
void ndNrtc::notifyObserver(INrtcObserver *obs, const char *state, const char *args)
{
    TRACE("notify observer");
    
    state_ = (char*)malloc(strlen(state)+1);
    args_ = (char*)malloc(strlen(args)+1);
    
    strcpy(state_, state);
    strcpy(args_,args);
    tempObserver_ = obs;
    
    nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(this, &ndNrtc::processEvent);
    NS_DispatchToMainThread(event);
}
void ndNrtc::processEvent()
{
    TRACE("processing event to JS: %s %s", state_, args_);
    
    tempObserver_->OnStateChanged(state_, args_);
    
    tempObserver_ = nullptr;
    free(state_);
    free(args_);
    state_ = 0;
    args_ = 0;
}
