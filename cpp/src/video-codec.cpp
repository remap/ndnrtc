//
//  video-codec.cpp
//  ndnrtc
//
//  Copyright 2013-2019 Regents of the University of California
//  For licensing details see the LICENSE file.
//
//  Author:  Peter Gusev
//  Created: 19/2/2019
//

#include "video-codec.hpp"
#include <vpx/vp8cx.h>
#include <vpx/vp8dx.h>

using namespace ndnrtc;
using namespace std;

//------------------------------------------------------------------------------
VideoCodec::Image::Image(const uint16_t& width, const uint16_t& height,
    const ImageFormat& format, uint8_t* data)
    : width_(width)
    , height_(height)
    , format_(format)
    , allocatedSz_(0)
    , isWrapped_(false)
    , data_(data)
    , uData_(nullptr)
{
    if (!data) allocate();
}

VideoCodec::Image::Image(const vpx_image_t *img)
    : allocatedSz_(0)
    , uData_(nullptr)
{
    wrap(img);
}

VideoCodec::Image::Image(const VideoCodec::Image& img)
{
    throw runtime_error("not implemented: "
                        "VideoCodec::Image::Image(const VideoCodec::Image& img)");
}

VideoCodec::Image::Image(VideoCodec::Image&& img)
{
    throw runtime_error("not implemented"
                        "VideoCodec::Image::Image(VideoCodec::Image&& img)");
}

VideoCodec::Image::~Image()
{
    if (allocatedSz_) free(data_);
}

int
VideoCodec::Image::read(FILE *file)
{
    uint8_t *buf = data_;

    if (format_ == ImageFormat::I420)
    {
        // for I420, strides are W W/2 W/2 and W
        for (int plane = 0; plane < 3; ++plane) {
            const int stride = strides_[plane];
            const int w = plane == 0 ? width_ : (width_+1) >> 1; // vpxImagePlaneWidth(img, plane) * ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
            const int h = plane == 0 ? height_ : (height_+1) >> 1; //vpxImagePlaneHeight(img, plane);

            for (int y = 0; y < h; ++y)
            {
                if (fread(buf, 1, w, file) != (size_t)w) return 0;
                buf += stride;
            }
        }
    }
    else
        throw runtime_error("unsupported image format");

    return 1;
}

int
VideoCodec::Image::write(FILE *file) const
{
    int res = 0;

    if (!isWrapped_)
    {
        size_t len = getAllocationSize();
        if ((res = fwrite((void*)data_, 1, len, file)) != len)
            return 0;
    }
    else
    {
        const vpx_image_t *img = &wrapper_;
        // write it plane by plane (borrowed from https://github.com/webmproject/libvpx/blob/a5d499e16570d00d5e1348b1c7977ced7af3670f/tools_common.c#L219)
        int plane;
        for (plane = 0; plane < 3; ++plane) {
          const unsigned char *buf = img->planes[plane];
          const int stride = img->stride[plane];
          const int w = vpxImgPlaneWidth(img, plane) *
                        ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
          const int h = vpxImgPlaneHeight(img, plane);
          int y;

          for (y = 0; y < h; ++y) {
            res = fwrite(buf, 1, w, file);
            buf += stride;

            if (res != w) return 0;
          }
        }
    }
    return 1;
}

int
VideoCodec::Image::copyTo(uint8_t* dataBuf) const
{
    int res = 0;

    if (!isWrapped_)
    {
        size_t len = getAllocationSize();
        memcpy((void*)dataBuf, (void*)data_, len);
    }
    else
    {
        const vpx_image_t *img = &wrapper_;
        // write it plane by plane (borrowed from https://github.com/webmproject/libvpx/blob/a5d499e16570d00d5e1348b1c7977ced7af3670f/tools_common.c#L219)
        int plane;
        size_t nCopied = 0;
        for (plane = 0; plane < 3; ++plane) {
            const unsigned char *buf = img->planes[plane];
            const int stride = img->stride[plane];
            const int w = vpxImgPlaneWidth(img, plane) *
            ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
            const int h = vpxImgPlaneHeight(img, plane);
            int y;

            for (y = 0; y < h; ++y) {
                memcpy((void*)dataBuf, (void*)buf, w);
                dataBuf += w;
                buf += stride;
                nCopied += w;
            }
        }
    }

    return 1;
}

void
VideoCodec::Image::wrap(const vpx_image_t *img)
{
    if (img->fmt == VPX_IMG_FMT_I420)
    {
        isWrapped_ = true;
        format_ = ImageFormat::I420;
        // NOTE: img->w and img->h may not be the resolution we are looking for
        // because VPX internally does memory block alignments,
        // thus we're using display width and height
        // TODO: what about r_w and r_h ?
        width_ = img->d_w;
        height_ = img->d_h;

        for (int i = 0; i < 4; ++i) strides_[i] = img->stride[i];
        uData_ = img->planes[0]; //img->user_priv;
        data_ = (uint8_t*)img;
        wrapper_ = *img;
    }
    else
        throw runtime_error("unsupported image format");
}

void
VideoCodec::Image::toVpxImage(vpx_image_t **img) const
{
    if (!isWrapped_)
    {
        // TODO: support other formats
        vpx_img_fmt_t imgFmt = (format_ == ImageFormat::I420 ? VPX_IMG_FMT_I420 : VPX_IMG_FMT_NONE);
        *img = vpx_img_wrap(*img, imgFmt, width_, height_, 1, data_);
    }
    else
    {
        *img = const_cast<vpx_image_t*>(&wrapper_);
    }
}

const vpx_image_t*
VideoCodec::Image::getVpxImage() const
{
    if (!isWrapped_)
        return nullptr;

    return &wrapper_;
}

size_t
VideoCodec::Image::getDataSize() const
{
    if (!isWrapped_)
    {
        if (allocatedSz_)
            return getAllocatedSize();
        else
            return getAllocationSize();
    }
    else
    {
        size_t sz = 0;
        const vpx_image_t *img = &wrapper_;
        int plane;
        for (plane = 0; plane < 3; ++plane) {
            const int w = vpxImgPlaneWidth(img, plane) *
                ((img->fmt & VPX_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
            const int h = vpxImgPlaneHeight(img, plane);
            sz += h*w;
        }

        return sz;
    }
}

const uint8_t*
VideoCodec::Image::yBuffer() const
{
    if (isWrapped_)
    {
        const vpx_image_t *img = &wrapper_;
        return img->planes[0];
    }
    return nullptr;
}

const uint8_t*
VideoCodec::Image::uBuffer() const
{
    if (isWrapped_)
    {
        const vpx_image_t *img = &wrapper_;
        return img->planes[1];
    }
    return nullptr;
}

const uint8_t*
VideoCodec::Image::vBuffer() const
{
    if (isWrapped_)
    {
        const vpx_image_t *img = &wrapper_;
        return img->planes[2];
    }
    return nullptr;
}

int
VideoCodec::Image::yStride() const
{
    if (isWrapped_)
    {
        const vpx_image_t *img = &wrapper_;
        return img->stride[0];
    }
    return 0;
}

int
VideoCodec::Image::uStride() const
{
    if (isWrapped_)
    {
        const vpx_image_t *img = &wrapper_;
        return img->stride[1];
    }
    return 0;
}

int
VideoCodec::Image::vStride() const
{
    if (isWrapped_)
    {
        const vpx_image_t *img = &wrapper_;
        return img->stride[2];
    }
    return 0;
}

void
VideoCodec::Image::allocate()
{
    if (format_ == ImageFormat::I420)
    {
        strides_[0] = strides_[3] = width_;
        strides_[1] = strides_[2] = width_/2;

        allocatedSz_ = getAllocationSize();
        data_ = (uint8_t*)malloc(allocatedSz_);

        if (!data_)
            throw runtime_error("failed to allocate memory for an image");
    }
}

size_t
VideoCodec::Image::getAllocationSize() const
{
    switch (format_)
    {
        case ImageFormat::I420:
            {
                return width_*height_*3/2;
            }
            break;
        default:
        {
            return 0;
        }
    }

    return 0;
}

int
VideoCodec::Image::vpxImgPlaneWidth(const vpx_image_t *img, int plane) const
{
  if (plane > 0 && img->x_chroma_shift > 0)
    return (img->d_w + 1) >> img->x_chroma_shift;
  else
    return img->d_w;
}

int
VideoCodec::Image::vpxImgPlaneHeight(const vpx_image_t *img, int plane) const
{
  if (plane > 0 && img->y_chroma_shift > 0)
    return (img->d_h + 1) >> img->y_chroma_shift;
  else
    return img->d_h;
}

//------------------------------------------------------------------------------
// vpx callbacks for external frame buffers
static int getFrameBuffer(void *user_priv, size_t min_size, vpx_codec_frame_buffer_t *fb)
{
    IFrameBufferList *fbList = reinterpret_cast<IFrameBufferList*>(user_priv);
    return fbList->getFreeFrameBuffer(min_size, fb);
}

static int releaseFrameBuffer(void *user_priv, vpx_codec_frame_buffer_t *fb)
{
    IFrameBufferList *fbList = reinterpret_cast<IFrameBufferList*>(user_priv);
    return fbList->returnFrameBuffer(fb);
}

VideoCodec::VideoCodec():
    raw_(nullptr)
    , isInit_(false)
    , incoming_(nullptr)
{
    memset((void*)&stats_, 0, sizeof(VideoCodec::Stats));
}

VideoCodec::~VideoCodec()
{
    if (raw_) delete raw_;
}

void VideoCodec::initEncoder(const CodecSettings &settings)
{
    if (isInit_)
        throw runtime_error("codec is already intialized");

    vpx_codec_enc_cfg_t cfg;
    vpx_codec_err_t res;
    vpx_codec_iface_t *codecIface = vpx_codec_vp9_cx();
    settings_ = settings;
    int w = settings_.spec_.encoder_.width_;
    int h = settings_.spec_.encoder_.height_;
    int bitrate = settings_.spec_.encoder_.bitrate_;
    int gop = settings_.spec_.encoder_.gop_;
    int fps =  settings_.spec_.encoder_.fps_;
    int dropFrames =  settings_.spec_.encoder_.dropFrames_;

    res = vpx_codec_enc_config_default(codecIface, &cfg, 0);
    if (res)
        throw runtime_error("failed to get default encoder config");

    cfg.g_pass = VPX_RC_ONE_PASS;
    cfg.g_lag_in_frames = 0;
    cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT; // 1
    cfg.rc_end_usage = VPX_CBR;
    cfg.rc_undershoot_pct = 95;
    cfg.rc_overshoot_pct = 5;
    cfg.rc_min_quantizer = 4;	 	// TODO: set like WebRTC?
    cfg.rc_max_quantizer = 56;
    cfg.rc_buf_initial_sz = 500;
    cfg.rc_buf_optimal_sz = 600;
    cfg.rc_buf_sz = 1000;
    cfg.rc_dropframe_thresh = dropFrames ? 30 : 0;
    cfg.g_threads = 8;	// TODO: do the same as WebRTC?

    cfg.g_w = w;
    cfg.g_h = h;
    cfg.g_timebase.num = 1;
    cfg.g_timebase.den = fps;		// TODO: set like WebRTC?
    cfg.rc_target_bitrate = bitrate;
    cfg.kf_min_dist = gop;
    cfg.kf_max_dist = gop;

    res = vpx_codec_enc_init(&vpxCodec_, codecIface, &cfg, 0);
    if (res)
        throw runtime_error("error initializing encoder");

    if (settings.rowMt_)
    {
        res = vpx_codec_control(&vpxCodec_, VP9E_SET_ROW_MT, 1);
        if (res)
            throw runtime_error("codec control error: failed to set row multithreading");
    }

    res = vpx_codec_control(&vpxCodec_, VP9E_SET_TILE_COLUMNS, 3);	// TODO: do the same as WebRTC?
    if (res)
        throw runtime_error("codec control error: failed to set column tiles");

    res = vpx_codec_control(&vpxCodec_, VP9E_SET_TILE_ROWS, 2);	// TODO: remove, like in WebRTC?
    if (res)
        throw runtime_error("codec control error: failed to set row tiles");

    res = vpx_codec_control(&vpxCodec_, VP8E_SET_MAX_INTRA_BITRATE_PCT, 130);	// TODO: tune like in WebRTC?
    if (res)
        throw runtime_error("codec control error: failed to set INTRA bitrate cap");

    res = vpx_codec_control_(&vpxCodec_, VP9E_SET_MAX_INTER_BITRATE_PCT, 200);   // TODO: tune like in WebRTC?
    if (res)
        throw runtime_error("codec control error: failed to set INTER bitrate cap");

    res = vpx_codec_control(&vpxCodec_, VP8E_SET_CPUUSED, 8); // TODO: tune like in WebRTC?
    if (res)
        throw runtime_error("codec control error: failed to set max cpu used");

    res  =vpx_codec_control(&vpxCodec_, VP9E_SET_FRAME_PARALLEL_DECODING, 0);
    if (res)
        throw runtime_error("codec control error: failed to set parallel decoding");

    res = vpx_codec_control(&vpxCodec_, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_DEFAULT);  // conferencing
    // vpx_codec_control(&codec, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_SCREEN); // screencapture
    //vpx_codec_control(&codec, VP9E_SET_TUNE_CONTENT, VP9E_CONTENT_FILM); // TouchDesigner
    if (res)
        throw runtime_error("codec control error: failed to set content type");

    res = vpx_codec_control(&vpxCodec_, VP8E_SET_STATIC_THRESHOLD, 1);
    if (res)
        throw runtime_error("codec control error: failed to set column tiles");

    raw_ = new VideoCodec::Image(cfg.g_w, cfg.g_h, ImageFormat::I420);
    isInit_ = true;
}

void VideoCodec::initDecoder(const CodecSettings &settings)
{
    if (isInit_)
        throw runtime_error("codec is already intialized");

    vpx_codec_dec_cfg_t cfg;
    vpx_codec_err_t res;
    memset(&cfg, 0, sizeof(cfg));
    vpx_codec_iface_t *codecIface = vpx_codec_vp9_dx();

    cfg.threads = std::min(settings.numCores_, 8u);
    res = vpx_codec_dec_init(&vpxCodec_, codecIface, &cfg, 0);
    if (res)
        throw runtime_error("error initializing decoder: "+to_string(res));

    if (settings.rowMt_)
    {
        res = vpx_codec_control(&vpxCodec_, VP9D_SET_ROW_MT, 1);
        if (res)
            throw runtime_error("codec control error: failed to set row-wise multithreading: "+to_string(res));
    }

    if (settings.spec_.decoder_.frameBufferList_)
    {
        res = vpx_codec_set_frame_buffer_functions(&vpxCodec_,
                getFrameBuffer, releaseFrameBuffer,
                settings.spec_.decoder_.frameBufferList_);
    }

    isInit_ = true;
}

int VideoCodec::encode(const VideoCodec::Image &rawFrame, bool forceIntraFrame,
                       VideoCodec::OnEncodedFrame onEncodedFrame,
                       VideoCodec::OnDroppedImage onDroppedImage)
{
    // TODO: implement forceIntraFrame or drop it completely
    rawFrame.toVpxImage(&incoming_);

    int got_pkts = 0;
    int flags = 0; // VPX_EFLAG_FORCE_KF;
    vpx_codec_iter_t iter = NULL;
    const vpx_codec_cx_pkt_t *pkt = NULL;

    stats_.bytesIn_ += rawFrame.getAllocationSize();
    const vpx_codec_err_t res =
        vpx_codec_encode(&vpxCodec_, incoming_, stats_.nFrames_++, 1, 0, VPX_DL_REALTIME);

    if (res != VPX_CODEC_OK)
        throw runtime_error("failed to encode frame");

    while ((pkt = vpx_codec_get_cx_data(&vpxCodec_, &iter)) != NULL) {
        got_pkts = 1;

        if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            const int keyFrame = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
            encoded_.type_ = keyFrame ? FrameType::Key : FrameType::Delta;
            encoded_.userData_ = rawFrame.getUserData();
            encoded_.pts_ = pkt->data.frame.pts;
            encoded_.duration_ = pkt->data.frame.duration;
            encoded_.length_ = pkt->data.frame.sz;
            encoded_.data_ = (uint8_t*)pkt->data.frame.buf;

            stats_.nProcessed_++;
            if (keyFrame) stats_.nKey_++;
            stats_.bytesOut_ += encoded_.length_;
            onEncodedFrame(encoded_);
        }
    }

    if (got_pkts == 0)
    {
        stats_.nDropped_++;
        onDroppedImage(rawFrame);
    }

    return 0;
}

int VideoCodec::decode(const EncodedFrame& encodedFrame,
                       VideoCodec::OnDecodedImage onDecodedImage)
{
    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = NULL;
    vpx_codec_err_t err;

    stats_.nFrames_++;
    stats_.bytesIn_ += encodedFrame.length_;
    if ((err = vpx_codec_decode(&vpxCodec_, encodedFrame.data_,
                        (unsigned int)encodedFrame.length_, NULL, 0)) != VPX_CODEC_OK)
        return err;

    while ((img = vpx_codec_get_frame(&vpxCodec_, &iter)) != NULL) {
        if (!raw_)
            raw_ = new VideoCodec::Image(img);
        else
            raw_->wrap(img);

        stats_.nProcessed_++;
        if (encodedFrame.type_ == FrameType::Key)
            stats_.nKey_++;
        stats_.bytesOut_ += raw_->getAllocationSize();
        onDecodedImage(*raw_);
    }

    return 0;
}

CodecSettings&
VideoCodec::defaultCodecSettings()
{
    static CodecSettings s {8, true, 1280, 720, 3000, 30, 30, true};
    return s;
}
