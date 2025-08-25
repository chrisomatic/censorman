extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base.h"

bool ffmpeg_decode(const char *filename, Video *output)
{
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    const AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVFrame *rgb_frame = NULL;
    AVPacket *pkt = NULL;
    struct SwsContext *sws_ctx = NULL;

    int video_stream_index = -1;
    int ret;
    u32 frame_count = 0;

    if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) < 0)
    {
        LOGE("Could not open input file '%s'", filename);
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        LOGE("Could not find stream info");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1)
    {
        LOGE("No video stream found");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    enum AVCodecID codec_id = fmt_ctx->streams[video_stream_index]->codecpar->codec_id;
    LOGI("Video codec id: %d (%s)\n", codec_id, avcodec_get_name(codec_id));

    codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_index]->codecpar->codec_id);
    if (!codec)
    {
        LOGE("Unsupported codec");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        LOGE("Could not allocate codec context");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    // Enable internal multithreading
    codec_ctx->thread_count = 0;  // 0 = auto
    codec_ctx->thread_type = FF_THREAD_FRAME; // or FF_THREAD_SLICE

    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
    {
        LOGE("Could not open codec");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int width = codec_ctx->width;
    int height = codec_ctx->height;
    int rgb_stride = width * 3;
    int frame_rgb_size = rgb_stride * height;

    // Allocate buffer for up to 1000 frames
    u8 *rgb_data = (u8 *)malloc((u64)frame_rgb_size * MAX_FRAMES);
    if (!rgb_data)
    {
        LOGE("Failed to allocate RGB buffer of size %lu", (u64)frame_rgb_size * MAX_FRAMES);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    pkt = av_packet_alloc();

    u8 *rgb_planes[4];
    int rgb_linesize[4];
    av_image_fill_arrays(rgb_planes, rgb_linesize, rgb_data, AV_PIX_FMT_RGB24, width, height, 1);

    sws_ctx = sws_getContext(width, height, codec_ctx->pix_fmt,
                             width, height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);

    while (av_read_frame(fmt_ctx, pkt) >= 0 && frame_count < MAX_FRAMES)
    {
        if (pkt->stream_index == video_stream_index)
        {
            ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0)
            {
                LOGE("Error sending packet for decoding");
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0)
                {
                    LOGE("Error during decoding");
                    goto cleanup_decode;
                }

                // Convert frame to RGB
                u8 *dest_data[4] = { rgb_data + frame_count * frame_rgb_size, NULL, NULL, NULL };
                int dest_linesize[4] = { rgb_stride, 0, 0, 0 };
                sws_scale(sws_ctx, (const u8 *const *)frame->data, frame->linesize, 0,
                          height, dest_data, dest_linesize);

                frame_count++;
                if (frame_count >= MAX_FRAMES)
                    break;
            }
        }
        av_packet_unref(pkt);
    }

    // Fill the output structure
    output->w = width;
    output->h = height;
    output->frame_count = frame_count;
    output->data = rgb_data;

    // Cleanup
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return true;

cleanup_decode:
    free(rgb_data);
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return false;
}

bool ffmpeg_encode(const char *filename, Video *video)
{
    AVFormatContext *fmt_ctx = NULL;
    AVStream *video_st = NULL;
    AVCodecContext *codec_ctx = NULL;
    const AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
    struct SwsContext *sws_ctx = NULL;
    int ret;

    int width = video->w;
    int height = video->h;

    // ---------------------------
    // Output format (MP4 / H264)
    // ---------------------------
    avformat_alloc_output_context2(&fmt_ctx, NULL, "mp4", filename);
    if (!fmt_ctx) {
        fprintf(stderr, "Could not deduce output format\n");
        return false;
    }

    codec = avcodec_find_encoder_by_name("libx264");

    if (!codec)
    {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    if (!codec)
    {
        codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    }

    if (!codec) {
        fprintf(stderr, "Encoder not found\n");
        avformat_free_context(fmt_ctx);
        return false;
    }

    // Add new video stream
    video_st = avformat_new_stream(fmt_ctx, NULL);
    if (!video_st) {
        fprintf(stderr, "Could not create stream\n");
        avformat_free_context(fmt_ctx);
        return false;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate codec context\n");
        avformat_free_context(fmt_ctx);
        return false;
    }

    // Basic encoding settings
    codec_ctx->codec_id = codec->id;
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = (AVRational){1, 30};   // 30 fps
    codec_ctx->framerate = (AVRational){30, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;      // encoder wants YUV420P
    codec_ctx->gop_size = 12;
    codec_ctx->max_b_frames = 2;
    codec_ctx->thread_count = 0; // 0 = auto-detect cores
    codec_ctx->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;

    if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary *opts = NULL;
    av_dict_set(&opts, "preset", "superfast", 0);   // ultrafast, superfast, fast, medium, slow, placebo
    av_dict_set(&opts, "tune", "zerolatency", 0);
    av_dict_set(&opts, "threads", "auto", 0);

    // Open encoder
    if ((ret = avcodec_open2(codec_ctx, codec, &opts)) < 0) {
        fprintf(stderr, "Could not open encoder\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return false;
    }

    // Copy codec params to stream
    ret = avcodec_parameters_from_context(video_st->codecpar, codec_ctx);
    if (ret < 0) {
        fprintf(stderr, "Could not copy codec parameters\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return false;
    }

    // Open output file
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open output file '%s'\n", filename);
            avcodec_free_context(&codec_ctx);
            avformat_free_context(fmt_ctx);
            return false;
        }
    }

    // Write header
    if (avformat_write_header(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Error occurred writing header\n");
        avio_close(fmt_ctx->pb);
        avcodec_free_context(&codec_ctx);
        avformat_free_context(fmt_ctx);
        return false;
    }

    int rgb_stride;

    // Allocate frame + packet
    frame = av_frame_alloc();
    pkt = av_packet_alloc();
    if (!frame || !pkt) {
        fprintf(stderr, "Could not allocate frame/packet\n");
        goto cleanup_encode;
    }

    frame->format = codec_ctx->pix_fmt;
    frame->width  = codec_ctx->width;
    frame->height = codec_ctx->height;
    if (av_frame_get_buffer(frame, 32) < 0) {
        fprintf(stderr, "Could not allocate frame buffer\n");
        goto cleanup_encode;
    }

    // SWS converter (RGB24 -> YUV420P)
    sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                             width, height, codec_ctx->pix_fmt,
                             SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Could not init sws context\n");
        goto cleanup_encode;
    }

    rgb_stride = width * 3;
    for (int i = 0; i < video->frame_count; i++) {
        const uint8_t *rgb_data[1] = { video->data + i * rgb_stride * height };
        int rgb_linesize[1] = { rgb_stride };

        av_frame_make_writable(frame);
        sws_scale(sws_ctx, rgb_data, rgb_linesize, 0, height, frame->data, frame->linesize);

        frame->pts = i;

        // Encode
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0) {
            fprintf(stderr, "Error sending frame\n");
            goto cleanup_encode;
        }

        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                fprintf(stderr, "Error encoding frame\n");
                goto cleanup_encode;
            }

            pkt->stream_index = video_st->index;
            av_packet_rescale_ts(pkt, codec_ctx->time_base, video_st->time_base);
            av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
        }
    }

    // Flush encoder
    avcodec_send_frame(codec_ctx, NULL);
    while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
        pkt->stream_index = video_st->index;
        av_packet_rescale_ts(pkt, codec_ctx->time_base, video_st->time_base);
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }

    // Write trailer
    av_write_trailer(fmt_ctx);

    // Cleanup
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(fmt_ctx->pb);
    avformat_free_context(fmt_ctx);

    return true;

cleanup_encode:

    if (sws_ctx) sws_freeContext(sws_ctx);
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (fmt_ctx) {
        if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE) && fmt_ctx->pb)
            avio_close(fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }
    return false;
}
