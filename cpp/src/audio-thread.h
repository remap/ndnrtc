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

#include "params.h"
#include "ndnrtc-object.h"
#include "audio-capturer.h"
#include "frame-data.h"

namespace ndnrtc
{
   class AudioBundlePacket;
   class AudioThread;
   
   class IAudioThreadCallback {
    public:
      virtual void onSampleBundle(std::string threadName, uint64_t bundleNo,
        boost::shared_ptr<AudioBundlePacket> packet) = 0;
   };

   class AudioThread :  public NdnRtcComponent,
                        public IAudioSampleConsumer
   {
   public:
       AudioThread(const AudioThreadParams& params,
           const AudioCaptureParams& captureParams,
           IAudioThreadCallback* callback,
           size_t bundleWireLength = 1000);
       ~AudioThread();

       void start();
       void stop();

       std::string getName() const { return threadName_; }
       bool isRunning() const { return isRunning_; }
       std::string getCodec() const { return codec_; }
       double getRate() const;

   private:
       AudioThread(const AudioThread&) = delete;

       uint64_t bundleNo_;
       uint32_t rateId_;
       std::string threadName_, codec_;
       IAudioThreadCallback* callback_;
       boost::shared_ptr<AudioBundlePacket> bundle_;
       AudioCapturer capturer_;
       boost::atomic<bool> isRunning_;

       void
       onDeliverRtpFrame(unsigned int len, uint8_t* data);

       void
       onDeliverRtcpFrame(unsigned int len, uint8_t* data);

       void 
       deliver(const AudioBundlePacket::AudioSampleBlob& blob);
   };
}

#endif /* defined(__ndnrtc__audio_thread__) */
