//
//  av-sync-test.cc
//  ndnrtc
//
//  Created by Peter Gusev on 1/21/14.
//  Copyright (c) 2014 Peter Gusev. All rights reserved.
//

#include "test-common.h"
#include "audio-sender.h"
#include "av-sync.h"
#include "camera-capturer.h"
#include "renderer.h"
#include <string.h>

#define USE_AVSYNC

// test audio/video by default are recorded with some offset
// it should be defined here
#define DEFAULT_AV_OFFSET 777

using namespace ndnrtc;
using namespace std;

::testing::Environment* const env = ::testing::AddGlobalTestEnvironment(new NdnRtcTestEnvironment(ENV_NAME));

class AudioVideoSynchronizerTester : public AudioVideoSynchronizer
{
public:
    AudioVideoSynchronizerTester(ParamsStruct videoParams, ParamsStruct audioParams)
    :AudioVideoSynchronizer(videoParams, audioParams){}
    
    int syncAudioPacket(int64_t packetTsRemote, int64_t packetTsLocal)
    {
        return syncPacket(audioSyncData_, videoSyncData_,
                          packetTsRemote, packetTsLocal, (NdnMediaReceiver*)0);
    }
    int syncVideoPacket(int64_t packetTsRemote, int64_t packetTsLocal)
    {
        return syncPacket(videoSyncData_, audioSyncData_,
                          packetTsRemote, packetTsLocal, (NdnMediaReceiver*)1);
    }
};

//******************************************************************************
class VideoFileRecorder : public IRawFrameConsumer
{
public:
    VideoFileRecorder(ParamsStruct &params, const char* fileName):
    cameraParams_(params),
    writer_(fileName),
    deliver_cs_(webrtc::CriticalSectionWrapper::CreateCriticalSection()),
    deliverEvent_(*webrtc::EventWrapper::Create()),
    completionEvent_(*webrtc::EventWrapper::Create()),
    processThread_(*webrtc::ThreadWrapper::CreateThread(processDeliveredFrame, this,
                                                        webrtc::kHighPriority))
    {
        obtainedFramesCount_ = 0;
    }
    
    void onDeliverFrame(webrtc::I420VideoFrame &frame)
    {
        deliver_cs_->Enter();
        obtainedFramesCount_++;
        deliverFrame_.SwapFrame(&frame);
        LOG_TRACE("captured frame %d %ld %dx%d", frame.timestamp(),
                  frame.render_time_ms(), frame.width(), frame.height());
        deliver_cs_->Leave();
        deliverEvent_.Set();
    }
    
    // grabs frames from camera and stores them in a file consequently
    void grabFrames(unsigned int seconds)
    {
        cc_ = new CameraCapturer(cameraParams_);
        
        cc_->init();
        cc_->setFrameConsumer(this);
        
        grabbingDuration_ = seconds*1000;
        startTime_ = NdnRtcUtils::millisecondTimestamp();
        
        unsigned int t;
        processThread_.Start(t);
        cc_->startCapture();
        
        completionEvent_.Wait(WEBRTC_EVENT_INFINITE);
        
        cc_->stopCapture();
        deliverEvent_.Set();
        processThread_.SetNotAlive();
        processThread_.Stop();
    }
    
protected:
    webrtc::scoped_ptr<webrtc::CriticalSectionWrapper> deliver_cs_;
    webrtc::ThreadWrapper &processThread_;
    webrtc::EventWrapper &deliverEvent_, &completionEvent_;
    webrtc::I420VideoFrame deliverFrame_;
    
    uint64_t startTime_, grabbingDuration_;
    int obtainedFramesCount_ = 0, byteCounter_ = 0;
    ParamsStruct cameraParams_;
    CameraCapturer *cc_;
    FrameWriter writer_;
    
    static bool processDeliveredFrame(void *obj) {
        return ((VideoFileRecorder*)obj)->process();
    }
    
    bool process()
    {
        uint64_t t = NdnRtcUtils::millisecondTimestamp();
        
        if (t-startTime_ >= grabbingDuration_)
        {
            completionEvent_.Set();
            return false;
        }
        
        if (deliverEvent_.Wait(100) == webrtc::kEventSignaled) {
            
            deliver_cs_->Enter();
            if (!deliverFrame_.IsZeroSize())
                writer_.writeFrame(deliverFrame_);
            //render_->onDeliverFrame(deliverFrame_);
            
            deliver_cs_->Leave();
        }
        
        return true;
    }
};
class AudioPacketWorker :  public webrtc::Transport
{
public:
    int nRtcp_ = 0, nRtp_ = 0;
    
    AudioPacketWorker():
    voiceEngine_(NdnRtcUtils::sharedVoiceEngine())
    {
        config_.Set<webrtc::AudioCodingModuleFactory>(new webrtc::NewAudioCodingModuleFactory());
        voiceEngine_ = webrtc::VoiceEngine::Create(config_);
        
        voe_base_ = webrtc::VoEBase::GetInterface(voiceEngine_);
        voe_network_ = webrtc::VoENetwork::GetInterface(voiceEngine_);
        voe_processing_ = webrtc::VoEAudioProcessing::GetInterface(voiceEngine_);
        voe_base_->Init();
    }
    virtual ~AudioPacketWorker(){}
    
    virtual int SendPacket(int channel, const void *data, int len)
    {
        // do nothing
        return len;
    }
    virtual int SendRTCPPacket(int channel, const void *data, int len)
    {
        // do nothing
        return len;
    }
    
protected:
    int channel_;
    webrtc::Config config_;
    webrtc::VoiceEngine *voiceEngine_;
    webrtc::VoEBase *voe_base_;
    webrtc::VoENetwork *voe_network_;
    webrtc::VoEAudioProcessing *voe_processing_;
};

class AudioFileRecorder :   public AudioPacketWorker
{
public:
    AudioFileRecorder(const char* fileName) :
    AudioPacketWorker(),
    audioWriter_(fileName)
    {
    }
    
    ~AudioFileRecorder(){}
    
    void grabFrames(unsigned int seconds)
    {
        int64_t startTime = NdnRtcUtils::millisecondTimestamp();
        
        channel_ = voe_base_->CreateChannel();
        
        ASSERT_LE(0, channel_);
        EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *this));
        EXPECT_EQ(0, voe_base_->StartReceive(channel_));
        EXPECT_EQ(0, voe_base_->StartSend(channel_));
        EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
        
        while (NdnRtcUtils::millisecondTimestamp()-startTime < seconds*1000)
        {
            usleep(10000);
        }
        
        EXPECT_EQ(0, voe_base_->StopSend(channel_));
        EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
        EXPECT_EQ(0, voe_base_->StopReceive(channel_));
        EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
    }
    
    int SendPacket(int channel, const void *data, int len)
    {
        nRtp_++;
        NdnAudioData::AudioPacket packet {false, NdnRtcUtils::millisecondTimestamp(),
            (unsigned int)len, NULL};
        
        packet.data_ = (unsigned char*)malloc(len);
        memcpy(packet.data_, data, len);
        
        audioWriter_.writePacket(packet);
        
        free(packet.data_);
        return len;
    }
    int SendRTCPPacket(int channel, const void *data, int len)
    {
        nRtcp_++;
        NdnAudioData::AudioPacket packet {true, NdnRtcUtils::millisecondTimestamp(),
            (unsigned int)len, (unsigned char*)data};
        
        audioWriter_.writePacket(packet);
        
        return len;
    }
    
protected:
    AudioWriter audioWriter_;
};

class AudioFileRenderer :   public AudioPacketWorker
{
public:
    AudioFileRenderer(const char *fileName) :
    AudioPacketWorker(),
    audioReader_(fileName)
    {}
    
    ~AudioFileRenderer()
    {
        
    }
    
    void setSynchronizer(AudioVideoSynchronizerTester *avsync)
    {
        avsync_ = avsync;
    }
    
    void playout(){
        NdnAudioData::AudioPacket packet, nextPacket;
        packet.data_ = (unsigned char*)malloc(500);
        nextPacket.data_ = (unsigned char*)malloc(500);
        memset(packet.data_, 0, 500);
        memset(nextPacket.data_, 0, 500);
        
        channel_ = voe_base_->CreateChannel();
        voe_processing_->SetEcStatus(true);
        
        ASSERT_LE(0, channel_);
        EXPECT_EQ(0, voe_network_->RegisterExternalTransport(channel_, *this));
        EXPECT_EQ(0, voe_base_->StartReceive(channel_));
        EXPECT_EQ(0, voe_base_->StartSend(channel_));
        EXPECT_EQ(0, voe_base_->StartPlayout(channel_));
        
        
        if (audioReader_.readPacket(packet) != -1)
        {
            int res = 0;
            while ((res = audioReader_.readPacket(nextPacket)) != -1)
            {
                int sleepTimeMs = nextPacket.timestamp_ - packet.timestamp_;
                
                if (packet.isRTCP_)
                {
                    nRtcp_++;
                    EXPECT_EQ(0, voe_network_->ReceivedRTCPPacket(channel_,
                                                                  packet.data_,
                                                                  packet.length_));
                }
                else
                {
                    nRtp_++;
                    EXPECT_EQ(0, voe_network_->ReceivedRTPPacket(channel_,
                                                                 packet.data_,
                                                                 packet.length_));
                }
                
                int adjustment = avsync_->syncAudioPacket(packet.timestamp_,
                                                          NdnRtcUtils::millisecondTimestamp());
                
#ifndef USE_AVSYNC
                adjustment = 0;
#endif
                
                usleep((sleepTimeMs+adjustment)*1000);
                
                unsigned char *tmp = packet.data_;
                
                packet = nextPacket;
                nextPacket.data_ = tmp;
            }
        }
        
        EXPECT_EQ(0, voe_base_->StopSend(channel_));
        EXPECT_EQ(0, voe_base_->StopPlayout(channel_));
        EXPECT_EQ(0, voe_base_->StopReceive(channel_));
        EXPECT_EQ(0, voe_network_->DeRegisterExternalTransport(channel_));
    }
    
protected:
    AudioReader audioReader_;
    AudioVideoSynchronizerTester *avsync_;
};

//******************************************************************************
class AVStreamWorker
{
public:
    AVStreamWorker():
    avSyncRWLock_(*webrtc::RWLockWrapper::CreateRWLock()),
    audioThread_(*webrtc::ThreadWrapper::CreateThread(AVStreamWorker::audioRoutine,
                                                      this,
                                                      webrtc::ThreadPriority:: kRealtimePriority,
                                                      "audio_thread")),
    videoThread_(*webrtc::ThreadWrapper::CreateThread(AVStreamWorker::videoRoutine,
                                                      this,
                                                      webrtc::ThreadPriority:: kRealtimePriority,
                                                      "video_thread"))
    {
        avSyncRWLock_.AcquireLockExclusive();
        
        unsigned int tid;
        
        audioThread_.Start(tid);
        videoThread_.Start(tid);
    }
    
    virtual ~AVStreamWorker()
    {
        audioThread_.SetNotAlive();
        videoThread_.SetNotAlive();
        
        audioThread_.Stop();
        videoThread_.Stop();
    }
    
protected:
    bool videoInProcess_ = false, audioInProcess_ = false;
    
    void startAVSynchronously()
    {
        avSyncRWLock_.ReleaseLockExclusive();
    }
    
    virtual bool audioProc()
    {
        return true;
    }
    virtual bool videoProc()
    {
        return true;
    }
    
private:
    webrtc::ThreadWrapper &audioThread_, &videoThread_;
    webrtc::RWLockWrapper &avSyncRWLock_;
    
    static bool audioRoutine(void *obj) {
        ((AVStreamWorker*)obj)->avSyncRWLock_.AcquireLockShared();
        bool res = ((AVStreamWorker*)obj)->audioProc();
        ((AVStreamWorker*)obj)->avSyncRWLock_.ReleaseLockShared();
        
        return res;
    }
    static bool videoRoutine(void *obj) {
        ((AVStreamWorker*)obj)->avSyncRWLock_.AcquireLockShared();
        bool res = ((AVStreamWorker*)obj)->videoProc();
        ((AVStreamWorker*)obj)->avSyncRWLock_.ReleaseLockShared();
        
        return res;
    }
};
// records YUV video and PCMU audio into files
class AVFileRecorder : public AVStreamWorker
{
public:
    AVFileRecorder(const char *audioFile, const char *videoFile,
                   ParamsStruct &videoParams) :
    AVStreamWorker(),
    arecorder_(audioFile),
    vrecorder_(videoParams, videoFile),
    videoParams_(videoParams)
    {
        audioInProcess_ = true;
        videoInProcess_ = true;
    }
    
    ~AVFileRecorder(){}
    
    void grabFrames(int seconds)
    {
        recordDurationSec_ = seconds;
        
        startAVSynchronously();
        
        while (videoInProcess_ || audioInProcess_)
        {
            usleep(10000);
        }
    }
    
protected:
    ParamsStruct videoParams_;
    VideoFileRecorder vrecorder_;
    AudioFileRecorder arecorder_;
    int recordDurationSec_;
    
    bool audioProc()
    {
        arecorder_.grabFrames(recordDurationSec_);
        audioInProcess_ = false;
        
        return audioInProcess_;
    }
    bool videoProc()
    {
        vrecorder_.grabFrames(recordDurationSec_);
        videoInProcess_ = false;
        
        return videoInProcess_;
    }
};

// renders YUV video with audio
class AVRenderer : public AVStreamWorker
{
public:
    AVRenderer(const char *audioFile, const char *videoFile,
               ParamsStruct &videoParams, ParamsStruct &audioParams) :
    AVStreamWorker(),
    videoParams_(videoParams),
    audioParams_(audioParams),
    synchronizer_(videoParams_, audioParams_),
    arenderer_(audioFile),
    vrenderer_(1, videoParams),
    vreader_(videoFile)
    {
        audioInProcess_ = true;
        videoInProcess_ = true;
        
        arenderer_.setSynchronizer(&synchronizer_);
        
        vrenderer_.init();
        vrenderer_.startRendering(videoFile);
    }
    
    ~AVRenderer()
    {
        vrenderer_.stopRendering();
    }
    
    void playout()
    {
        startAVSynchronously();
        
        while (audioInProcess_ || videoInProcess_)
        {
            usleep(10000);
        }
    }
    
    void setAudioOffset(int audioOffsetMs)
    {
        audioOffsetMs_ = audioOffsetMs;
    }
    
    void setVideoOffset(int videoOffsetMs)
    {
        videoOffsetMs_ = videoOffsetMs;
    }
    
protected:
    int audioOffsetMs_ = 0, videoOffsetMs_ = 0;
    
    AudioVideoSynchronizerTester synchronizer_;
    ParamsStruct videoParams_, audioParams_;
    AudioFileRenderer arenderer_;
    NdnRenderer vrenderer_;
    FrameReader vreader_;
    
    bool audioProc()
    {
        arenderer_.playout();
        
        audioInProcess_ = false;
        return audioInProcess_;
    }
    
    bool videoProc()
    {
        webrtc::I420VideoFrame frame, nextFrame;
        unsigned int nFrames = 0;
        
        if (videoOffsetMs_)
            usleep(videoOffsetMs_*1000);
        
        if (vreader_.readFrame(frame) >= 0)
        {
            while (vreader_.readFrame(nextFrame) >= 0)
            {
                // use saved render time for calculating rendering delay
                int sleepMs = nextFrame.render_time_ms()-frame.render_time_ms();
                int64_t remoteTs = frame.render_time_ms();
                
                nFrames++;
                // set current render timestamp (otherwise renderer will not work)
                frame.set_render_time_ms(webrtc::TickTime::MillisecondTimestamp());
                
                vrenderer_.onDeliverFrame(frame);
                frame.SwapFrame(&nextFrame);
                
                int adjustment = synchronizer_.syncVideoPacket(remoteTs,
                                                               NdnRtcUtils::millisecondTimestamp());
#ifndef USE_AVSYNC
                adjustment = 0;
#endif
                usleep((sleepMs+adjustment)*1000);
            } // while
        } // if
        
        videoInProcess_ = false;
        return videoInProcess_;
    } // videoProc
};

class AVSyncTester : public NdnRtcObjectTestHelper
{
public:
    void SetUp()
    {
        areader_ = new AudioReader("resources/bipbop30.pcmu");
        vreader_ = new FrameReader("resources/bipbop30.yuv");
        
        avSync_ = new AudioVideoSynchronizerTester(DefaultParams, DefaultParamsAudio);
    }
    
    void TearDown()
    {
        delete areader_;
        delete vreader_;
    }
    
    void testSync()
    {
        webrtc::I420VideoFrame frame, nextFrame;
        NdnAudioData::AudioPacket audioSample, nextSample;
        
        audioSample.data_ = (unsigned char*)malloc(500);
        nextSample.data_ = (unsigned char*)malloc(500);
        memset(audioSample.data_, 0, 500);
        memset(nextSample.data_, 0, 500);
        
        bool canProcess = vreader_->readFrame(frame) >= 0 &&
        areader_->readPacket(audioSample) >= 0 &&
        vreader_->readFrame(nextFrame) >= 0 &&
        areader_->readPacket(nextSample) >= 0;
        
        bool playingAudio = true, playingVideo = true;
        bool audioStopped = false, videoStopped = false;
        
        int64_t playoutTime = 0;
        int64_t nextAudioPlayoutTime = playoutTime;
        int64_t nextVideoPlayoutTime = playoutTime;
        int64_t videoRemotePlayoutTime = frame.render_time_ms()+vOffset_,
        audioRemotePlayoutTime = audioSample.timestamp_+aOffset_;
        // simulated packet arrival delays
        int64_t audioPacketDelay = 0, videoPacketDelay = 0;
        int iter = 0;
        
        while (canProcess)
        {
            int audioSyncAdjusted = 0, videoSyncAdjusted = 0;
            
            audioRemotePlayoutTime = audioSample.timestamp_+aOffset_;
            videoRemotePlayoutTime = frame.render_time_ms()+vOffset_;

            
            if (!audioStopped && playingAudio)
            {
                int audioPlayoutTime = nextSample.timestamp_ - audioSample.timestamp_;
                audioSyncAdjusted = avSync_->syncAudioPacket(audioRemotePlayoutTime,
                                                             nextAudioPlayoutTime);
                
                nextAudioPlayoutTime += audioPlayoutTime;
                nextAudioPlayoutTime += audioSyncAdjusted;
            }
            
            if (!videoStopped && playingVideo)
            {
                int videoPlayoutTime = nextFrame.render_time_ms() - frame.render_time_ms();
                videoSyncAdjusted = avSync_->syncVideoPacket(videoRemotePlayoutTime,
                                                             nextVideoPlayoutTime);
                
                nextVideoPlayoutTime += videoPlayoutTime;
                nextVideoPlayoutTime += videoSyncAdjusted;
            }
            
            //***
            // how drift is calculated?
            // 1. each stream has two timelines:
            //  - remote timeline (i.e. packet timestamp which were provided by
            //      producer)
            //  - local timeline (i.e. timestamps for the packets provided by
            //      the local machine - consumer)
            // 2. local timelines for two streams coincide
            // 3. in order to check the drift, we need 1) translate local
            // timestamp of the current packet to the remote timeline of the
            // adjacent stream and 2) calculate the difference between
            // translated value and actual value of the packets' remote
            // timestamp. this value will define a drift between the two streams
            int drift = (playingAudio)?
            (audioRemotePlayoutTime-playoutTime)+nextVideoPlayoutTime-(nextFrame.render_time_ms()+vOffset_):
            (videoRemotePlayoutTime-playoutTime)+
            nextAudioPlayoutTime-(nextSample.timestamp_+aOffset_);
            
            // after 2nd iteration, synchronization should work for sure
            // 1st iteration - could be for initializations only
            // 2nd iteration - could perform synchronization only if audio is
            // falling behind video (because we asking to synchronize audio packet
            // first). also, check if audio/video playbacks have not stopped
            if (iter > 2 && !(audioStopped || videoStopped))
            {
                EXPECT_EQ(0, drift);
            }
            //***
            
            if (nextAudioPlayoutTime < nextVideoPlayoutTime)
                playoutTime = (audioStopped)?nextVideoPlayoutTime:nextAudioPlayoutTime;
            else
                playoutTime = (videoStopped)?nextAudioPlayoutTime:nextVideoPlayoutTime;
            
            playingAudio = nextAudioPlayoutTime == playoutTime && !audioStopped;
            playingVideo = nextVideoPlayoutTime == playoutTime && !videoStopped;
            canProcess = false;
            
            //        cout << ((playoutTime == nextVideoPlayoutTime)?"V: ":"A: ")
            //                << playoutTime << " drift (a-v): " << drift << endl;
            
            if (playingAudio)
            {
                unsigned char *tmp = audioSample.data_;
                audioSample = nextSample;
                nextSample.data_ = tmp;
                
                audioStopped = (areader_->readPacket(nextSample) < 0);
            }
            if (playingVideo)
            {
                frame.SwapFrame(&nextFrame);
                
                videoStopped = (vreader_->readFrame(nextFrame) < 0);
            }
            
            canProcess = !(audioStopped && videoStopped);
            iter++;
        }
    }
    
protected:
    int aOffset_ = 0, vOffset_ = 0;
    FrameReader *vreader_;
    AudioReader *areader_;
    AudioVideoSynchronizerTester *avSync_;
};
#if 0
TEST(AVSyncTest, PlayoutAV)
{
    ParamsStruct p = DefaultParams;
    p.captureDeviceId = 0;
    AVRenderer avRenderer("resources/bipbop30.pcmu", "resources/bipbop30.yuv", p);
    
    //    avRenderer.setVideoOffset(1000);
    avRenderer.setAudioOffset(1000);
    avRenderer.playout();
    
#if 0
    ParamsStruct p = DefaultParams;
    p.captureDeviceId = 0;
    
    AVFileRecorder avRecorder("resources/bipbop30.pcmu", "resources/bipbop30.yuv", p);
    
    avRecorder.grabFrames(30);
#endif
#if 0
    webrtc::Trace::CreateTrace();
    webrtc::Trace::SetTraceFile("bin/webrtc-audio.log");
    
    AudioFileRenderer arenderer("resources/bipbop.pcmu");
    
    arenderer.playout();
    cout << "played out RTP packets: " << arenderer.nRtp_ << " RTCP packets: " << arenderer.nRtcp_ << endl;
#endif
#if 0
    ParamsStruct ap = DefaultParamsAudio;
    AudioFileRecorder arecorder(ap, "resources/bipbop.pcmu");
    
    arecorder.grabFrames(10);
    
    cout << "recorded RTP packets: " << arecorder.nRtp_ << " RTCP packets: " << arecorder.nRtcp_ << endl;
#endif
#if 0
    ParamsStruct p = DefaultParams;
    p.captureDeviceId = 0;
    
    VideoFileRecorder recorder(p, "resources/bipbop.yuv");
    
    recorder.grabFrames(30);
#endif
}
#endif

TEST_F(AVSyncTester, TestAVSynchronizerNormal)
{
    aOffset_ = DEFAULT_AV_OFFSET;
    vOffset_ = 0;
    
    testSync();
}
TEST_F(AVSyncTester, TestAVSynchronizerAudioLate)
{
    aOffset_ = DEFAULT_AV_OFFSET+((double)DefaultParamsAudio.jitterSize)/2.;
    vOffset_ = 0;
    
    testSync();
}
TEST_F(AVSyncTester, TestAVSynchronizerVideoLate)
{
    aOffset_ = DEFAULT_AV_OFFSET;
    vOffset_ = ((double)DefaultParams.jitterSize)/2.;
    
    testSync();
}
TEST_F(AVSyncTester, TestAVSynchronizerReset)
{
    aOffset_ = DEFAULT_AV_OFFSET;
    vOffset_ = 50;
    testSync();
    
    avSync_->reset();
    
    aOffset_ = DEFAULT_AV_OFFSET+50;
    vOffset_ = 0;
    testSync();
}
