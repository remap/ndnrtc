//
//  audio-sender.h
//  ndnrtc
//
//  Copyright 2013 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//

#ifndef __ndnrtc__audio_thread__
#define __ndnrtc__audio_thread__

#include <boost/thread.hpp>
#include <ndn-cpp/data.hpp>

#include "params.hpp"
#include "ndnrtc-object.hpp"
#include "audio-capturer.hpp"
#include "frame-data.hpp"
#include "estimators.hpp"

namespace ndnrtc
{
struct Mutable;
template <typename T>
class AudioBundlePacketT;
class AudioThread;

class IAudioThreadCallback
{
  public:
    /**
     * This is called when audio bundle consisting of RTP/RTCP packets
     * is ready.
     * @param threadName Name of the audio media thread
     * @param bundleNo Sequential bundle number
     * @param packet Shared pointer for bundle packet
     * @note This is called on audio system thread
     * @see AudioController
     */
    virtual void onSampleBundle(std::string threadName, uint64_t bundleNo,
                                boost::shared_ptr<AudioBundlePacketT<Mutable>> packet) = 0;
};

class AudioThread : public NdnRtcComponent,
                    public IAudioSampleConsumer
{
  public:
    AudioThread(const AudioThreadParams &params,
                const AudioCaptureParams &captureParams,
                IAudioThreadCallback *callback,
                size_t bundleWireLength = 1000);
    ~AudioThread();

    void start();
    void stop();

    std::string getName() const { return threadName_; }
    bool isRunning() const { return isRunning_; }
    std::string getCodec() const { return codec_; }
    double getRate() const;
    uint64_t getBundleNo() const { return bundleNo_; }
    void setLogger(boost::shared_ptr<ndnlog::new_api::Logger> logger);

  private:
    AudioThread(const AudioThread &) = delete;

    uint64_t bundleNo_;
    estimators::FreqMeter rateMeter_;
    std::string threadName_, codec_;
    IAudioThreadCallback *callback_;
    boost::shared_ptr<AudioBundlePacketT<Mutable>> bundle_;
    AudioCapturer capturer_;
    boost::atomic<bool> isRunning_;

    void
    onDeliverRtpFrame(unsigned int len, uint8_t *data);

    void
    onDeliverRtcpFrame(unsigned int len, uint8_t *data);

    void
    deliver(const AudioBundlePacketT<Mutable>::AudioSampleBlob &blob);
};
}

#endif /* defined(__ndnrtc__audio_thread__) */
