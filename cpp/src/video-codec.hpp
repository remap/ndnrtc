//
//  video-codec.hpp
//  ndnrtc
//
//  Copyright 2013-2019 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 19/2/2019
//

#ifndef __video_encoder_hpp__
#define __video_encoder_hpp__

#if HAVE_LIBVPX
#include <vpx/vpx_encoder.h>
#include <vpx/vpx_decoder.h>
#endif

#include "ndnrtc-object.hpp"
#include "ndnrtc-common.hpp"

namespace ndnrtc
{

/**
 * Video codec class. Implements compile-time configurable video codec.
 * Default is VP9.
 * This class can be initialized as either encoder or decoder. Hence, when
 * initialized as encoder, "decode" (and decoder-related) methods will throw.
 * Same holds when initialized as decoder.
 */
class VideoCodec : public NdnRtcComponent {
public:

  class Image {
  public:
      Image(const uint16_t& width, const uint16_t& height,
            const ImageFormat& format, uint8_t *data = nullptr);
      // non-copying, wrapping
      Image(const vpx_image_t *img);
      Image(const Image&);
      Image(Image&&);
      ~Image();

      int read(FILE *file);
      int write(FILE *file) const;
      int copyTo(uint8_t*) const;

      #if HAVE_LIBVPX
            void wrap(const vpx_image_t *img);
            void toVpxImage(vpx_image_t **img) const;
            // If getIsWrapped() == true, this returns underlying vpx_image_t
            // pointer.
            const vpx_image_t* getVpxImage() const;
      #endif

      uint16_t getWidth() const { return width_; }
      uint16_t getHeight() const { return height_; }
      ImageFormat getFormat() const { return format_; }

      // calculates size of allocation necessary for this image to store data
      // according to its format
      size_t getAllocationSize() const;
      // returns size of actually allocated data
      size_t getAllocatedSize() const { return allocatedSz_; }
      bool getIsWrapped() const { return isWrapped_; }
      size_t getDataSize() const;

      void setUserData(void *uData) { uData_ = uData; }
      void* getUserData() const { return uData_; }

  private:
      uint16_t width_, height_;
      ImageFormat format_;
      uint8_t *data_;
      void *uData_;
      int strides_[4];
      size_t allocatedSz_;
      bool isWrapped_;
      vpx_image_t wrapper_;

      void allocate();
      int vpxImgPlaneWidth(const vpx_image_t *img, int plane) const;
      int vpxImgPlaneHeight(const vpx_image_t *img, int plane) const;
  };

  typedef struct _Stats {
      uint32_t nFrames_, nProcessed_, nDropped_, nKey_;
      uint64_t bytesIn_, bytesOut_;
      double procDelayAvg_, procDelayJitter_;
  } Stats;

  typedef std::function<void(const EncodedFrame&)> OnEncodedFrame;
  typedef std::function<void(const Image&)> OnDroppedImage;
  typedef OnDroppedImage OnDecodedImage;

  VideoCodec();
  // TODO make sure structures are properly released
  ~VideoCodec();

  /**
   * Initializes encoder
   * @param settings Encoder settings.
   */
  void initEncoder(const CodecSettings& settings);

  /**
   * Initializes decoder
   * @param settings Decoder settings.
   */
  void initDecoder(const CodecSettings& settings);

  /**
   * Encodes raw frame.
   * The method is not thread-safe.
   * Client code should process returned result before calling this method again.
   * @param rawFrame Raw frame to encode
   * @param forceIntraFrame Forces encoder to produce intra frame.
   * @param onEncodedFrame Callback called when frame is succesfully encoded.
   *                       Client code should not hold references for the passed
   *                       object. It will become invalid on the next encode call.
   * @param onDroppedImage Callback called if frame was dropped by encoder.
   * @return 0 if frame was encoded, error code if there was an error. The call
   *         will return after either of passed callbacks triggered or when
   *         there is an error.
   * @throws If initialized as decoder.
   */
  int encode(const Image& rawFrame, bool forceIntraFrame,
        OnEncodedFrame onEncodedFrame, OnDroppedImage onDroppedImage);

  /**
   * Decodes encoded frames
   * @param encodedFrame Encoded frame
   * @param onDeocdedImage Callback called when frame is succesfully deocded.
   * @return 0 if frame was decoded successfully, error code otherwise.
   */
  int decode(const EncodedFrame& encodedFrame, OnDecodedImage onDecodedImage);

  const CodecSettings& getSettings() const { return settings_; }
  const Stats& getStats() const { return stats_; }

  static CodecSettings& defaultCodecSettings();

private:
    CodecSettings settings_;
    Stats stats_;
    EncodedFrame encoded_;
    Image *raw_;
    bool isInit_;

#if HAVE_LIBVPX
    vpx_codec_ctx_t vpxCodec_;
    vpx_image_t *incoming_;
#endif

};

#if HAVE_LIBVPX
class IFrameBufferList {
public:
    virtual ~IFrameBufferList(){}
    virtual int getFreeFrameBuffer(size_t minSize, vpx_codec_frame_buffer_t *fb) = 0;
    virtual int returnFrameBuffer(vpx_codec_frame_buffer_t *fb) = 0;
    virtual size_t getUsedBufferNum() = 0;
    virtual size_t getFreeBufferNum() = 0;
};
#endif

}

#endif
