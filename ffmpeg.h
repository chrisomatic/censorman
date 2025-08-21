#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
}

#include <stdio.h>

#include "base.h"
#include "transform.h"
#include "util.h"

static void process_frame_rgb_inplace(AVFrame *rgb) {
    // Stub: invert colors (replace with your CNN + blur)
    int w = rgb->width, h = rgb->height, ls = rgb->linesize[0];
    uint8_t *p = rgb->data[0];
    for (int y = 0; y < h; ++y) {
        uint8_t *row = p + y*ls;
        for (int x = 0; x < w*3; ++x) row[x] = 255 - row[x];
    }
}

static enum AVPixelFormat normalize_dec_fmt(enum AVPixelFormat f) {
    // Avoid deprecated full-range YUVJ* warnings in sws
    if (f == AV_PIX_FMT_YUVJ420P) return AV_PIX_FMT_YUV420P;
    if (f == AV_PIX_FMT_YUVJ422P) return AV_PIX_FMT_YUV422P;
    if (f == AV_PIX_FMT_YUVJ444P) return AV_PIX_FMT_YUV444P;
    return f;
}

static int encode_and_write(AVCodecContext *enc_ctx, AVStream *out_st, AVFrame *frm, AVFormatContext *out_fmt) {
    int ret = avcodec_send_frame(enc_ctx, frm);
    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "encode: send_frame error %d\n", ret);
        return ret;
    }

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) return AVERROR(ENOMEM);

    for (;;) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) { fprintf(stderr, "encode: receive_packet error %d\n", ret); break; }

        av_packet_rescale_ts(pkt, enc_ctx->time_base, out_st->time_base);
        pkt->stream_index = out_st->index;

        int wret = av_interleaved_write_frame(out_fmt, pkt);
        if (wret < 0) { fprintf(stderr, "mux: write_frame error %d\n", wret); ret = wret; break; }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
    return (ret == AVERROR(EAGAIN)) ? 0 : ret;
}

bool ffmpeg_process_video(const char* infile, const char* outfile)
{
    AVFormatContext *in_fmt = NULL;
    if (avformat_open_input(&in_fmt, infile, NULL, NULL) < 0) { fprintf(stderr,"open input failed\n"); return false; }
    if (avformat_find_stream_info(in_fmt, NULL) < 0) { fprintf(stderr,"find_stream_info failed\n"); return false; }

    int v_idx = av_find_best_stream(in_fmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (v_idx < 0) { fprintf(stderr,"no video stream\n"); return false; }
    AVStream *in_st = in_fmt->streams[v_idx];

    // Decoder
    const AVCodec *dec = avcodec_find_decoder(in_st->codecpar->codec_id);
    if (!dec) { fprintf(stderr,"decoder not found\n"); return false; }
    AVCodecContext *dec_ctx = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(dec_ctx, in_st->codecpar);
    // optional: dec_ctx->thread_count = 0; // auto
    if (avcodec_open2(dec_ctx, dec, NULL) < 0) { fprintf(stderr,"open decoder failed\n"); return false; }

    // Output
    AVFormatContext *out_fmt = NULL;
    if (avformat_alloc_output_context2(&out_fmt, NULL, NULL, outfile) < 0 || !out_fmt) { fprintf(stderr,"alloc output context failed\n"); return false; }

    const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!enc) { fprintf(stderr,"H.264 encoder not found\n"); return false; }

    AVStream *out_st = avformat_new_stream(out_fmt, NULL);
    if (!out_st) { fprintf(stderr,"new stream failed\n"); return false; }

    // Framerate + time bases
    AVRational fr = av_guess_frame_rate(in_fmt, in_st, NULL);
    if (fr.num == 0 || fr.den == 0) { fr.num = 30; fr.den = 1; } // fallback
    AVRational enc_tb = av_inv_q(fr);

    AVCodecContext *enc_ctx = avcodec_alloc_context3(enc);
    enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    enc_ctx->width  = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    enc_ctx->time_base = enc_tb;           // encoder time base
    enc_ctx->pkt_timebase = enc_tb;        // important for muxing
    enc_ctx->framerate = fr;
    enc_ctx->gop_size = 2 * fr.num / fr.den;   // ~2 seconds GOP
    enc_ctx->max_b_frames = 2;
    enc_ctx->bit_rate = 2'000'000;         // modest default

    if (out_fmt->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(enc_ctx, enc, NULL) < 0) { fprintf(stderr,"open encoder failed\n"); return false; }

    // Make muxer stream match encoder params and time base
    if (avcodec_parameters_from_context(out_st->codecpar, enc_ctx) < 0) { fprintf(stderr,"params_from_context failed\n"); return false; }
    out_st->time_base = enc_ctx->time_base;

    if (!(out_fmt->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_fmt->pb, outfile, AVIO_FLAG_WRITE) < 0) { fprintf(stderr,"avio_open failed\n"); return false; }
    }
    if (avformat_write_header(out_fmt, NULL) < 0) { fprintf(stderr,"write_header failed\n"); return false; }

    // Scalers
    enum AVPixelFormat dec_pix = normalize_dec_fmt(dec_ctx->pix_fmt);
    struct SwsContext *to_rgb = sws_getContext(
        dec_ctx->width, dec_ctx->height, dec_pix,
        dec_ctx->width, dec_ctx->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL);

    struct SwsContext *to_yuv = sws_getContext(
        dec_ctx->width, dec_ctx->height, AV_PIX_FMT_RGB24,
        enc_ctx->width, enc_ctx->height, enc_ctx->pix_fmt,
        SWS_BILINEAR, NULL, NULL, NULL);

    // Frames/buffers
    AVFrame *dec_fr = av_frame_alloc();
    AVFrame *rgb_fr = av_frame_alloc();
    AVFrame *yuv_fr = av_frame_alloc();
    if (!dec_fr || !rgb_fr || !yuv_fr) { fprintf(stderr,"frame alloc failed\n"); return false; }

    int rgb_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, dec_ctx->width, dec_ctx->height, 1);
    uint8_t *rgb_buf = (uint8_t *)av_malloc(rgb_size);
    av_image_fill_arrays(rgb_fr->data, rgb_fr->linesize, rgb_buf, AV_PIX_FMT_RGB24, dec_ctx->width, dec_ctx->height, 1);
    rgb_fr->format = AV_PIX_FMT_RGB24; rgb_fr->width = dec_ctx->width; rgb_fr->height = dec_ctx->height;

    int yuv_size = av_image_get_buffer_size(enc_ctx->pix_fmt, enc_ctx->width, enc_ctx->height, 1);
    uint8_t *yuv_buf = (uint8_t *)av_malloc(yuv_size);
    av_image_fill_arrays(yuv_fr->data, yuv_fr->linesize, yuv_buf, enc_ctx->pix_fmt, enc_ctx->width, enc_ctx->height, 1);
    yuv_fr->format = enc_ctx->pix_fmt; yuv_fr->width = enc_ctx->width; yuv_fr->height = enc_ctx->height;

    // Read/Decode/Process/Encode
    AVPacket *in_pkt = av_packet_alloc();
    if (!in_pkt) { fprintf(stderr,"in pkt alloc failed\n"); return false; }

    int ret = 0;
    while ((ret = av_read_frame(in_fmt, in_pkt)) >= 0) {
        if (in_pkt->stream_index == v_idx) {
            if ((ret = avcodec_send_packet(dec_ctx, in_pkt)) < 0) { fprintf(stderr,"send_packet err %d\n", ret); break; }
            for (;;) {
                ret = avcodec_receive_frame(dec_ctx, dec_fr);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                if (ret < 0) { fprintf(stderr,"receive_frame err %d\n", ret); goto done; }

                // To RGB for your CNN/blur
                sws_scale(to_rgb, (const uint8_t* const*)dec_fr->data, dec_fr->linesize, 0, dec_ctx->height, rgb_fr->data, rgb_fr->linesize);
                process_frame_rgb_inplace(rgb_fr);

                // Back to YUV420 for encoder
                sws_scale(to_yuv, (const uint8_t* const*)rgb_fr->data, rgb_fr->linesize, 0, rgb_fr->height, yuv_fr->data, yuv_fr->linesize);

                // Timestamp: rescale best-effort PTS from input stream TB -> encoder TB
                int64_t best_ts = (dec_fr->pts == AV_NOPTS_VALUE)
                    ? dec_fr->pkt_dts   // fallback
                    : dec_fr->pts;
                if (best_ts == AV_NOPTS_VALUE) {
                    // fallback to running index in encoder TB
                    static int64_t idx = 0;
                    yuv_fr->pts = idx++;
                } else {
                    yuv_fr->pts = av_rescale_q(best_ts, in_st->time_base, enc_ctx->time_base);
                }

                // Encode & write
                ret = encode_and_write(enc_ctx, out_st, yuv_fr, out_fmt);
                if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) { fprintf(stderr,"encode/write err %d\n", ret); goto done; }

                av_frame_unref(dec_fr);
            }
        }
        av_packet_unref(in_pkt);
    }

    // --- Flush decoder ---
    avcodec_send_packet(dec_ctx, NULL);
    for (;;) {
        ret = avcodec_receive_frame(dec_ctx, dec_fr);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) break;
        if (ret < 0) { fprintf(stderr,"decoder flush err %d\n", ret); break; }

        sws_scale(to_rgb, (const uint8_t* const*)dec_fr->data, dec_fr->linesize, 0, dec_ctx->height, rgb_fr->data, rgb_fr->linesize);
        process_frame_rgb_inplace(rgb_fr);
        sws_scale(to_yuv, (const uint8_t* const*)rgb_fr->data, rgb_fr->linesize, 0, rgb_fr->height, yuv_fr->data, yuv_fr->linesize);

        int64_t best_ts = (yuv_fr->pts == AV_NOPTS_VALUE)
                    ? yuv_fr->pkt_dts   // fallback
                    : yuv_fr->pts;
        yuv_fr->pts = (best_ts == AV_NOPTS_VALUE)
            ? (yuv_fr->pts + 1)
            : av_rescale_q(best_ts, in_st->time_base, enc_ctx->time_base);

        ret = encode_and_write(enc_ctx, out_st, yuv_fr, out_fmt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) { fprintf(stderr,"encode/write err %d\n", ret); break; }

        av_frame_unref(dec_fr);
    }

    // --- Flush encoder ---
    encode_and_write(enc_ctx, out_st, NULL, out_fmt);

    av_write_trailer(out_fmt);

done:
    // Cleanup
    av_packet_free(&in_pkt);
    av_frame_free(&dec_fr);
    av_frame_free(&rgb_fr);
    av_frame_free(&yuv_fr);
    av_free(rgb_buf);
    av_free(yuv_buf);
    sws_freeContext(to_rgb);
    sws_freeContext(to_yuv);
    avcodec_free_context(&dec_ctx);
    avcodec_free_context(&enc_ctx);
    if (!(out_fmt->oformat->flags & AVFMT_NOFILE)) avio_closep(&out_fmt->pb);
    avformat_close_input(&in_fmt);
    avformat_free_context(out_fmt);

    return true;
}


