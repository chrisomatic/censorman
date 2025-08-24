#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
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
        fprintf(stderr, "Could not open input file '%s'\n", filename);
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
    {
        fprintf(stderr, "Could not find stream info\n");
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
        fprintf(stderr, "No video stream found\n");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_index]->codecpar->codec_id);
    if (!codec)
    {
        fprintf(stderr, "Unsupported codec\n");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "Could not allocate codec context\n");
        avformat_close_input(&fmt_ctx);
        return false;
    }

    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    // Enable internal multithreading
    codec_ctx->thread_count = 0;  // 0 = auto
    codec_ctx->thread_type = FF_THREAD_FRAME; // or FF_THREAD_SLICE

    if (avcodec_open2(codec_ctx, codec, NULL) < 0)
    {
        fprintf(stderr, "Could not open codec\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int width = codec_ctx->width;
    int height = codec_ctx->height;
    int rgb_stride = width * 3;
    int frame_rgb_size = rgb_stride * height;

    // Allocate buffer for up to 1000 frames
    u8 *rgb_data = (u8 *)malloc(frame_rgb_size * MAX_FRAMES);
    if (!rgb_data)
    {
        fprintf(stderr, "Failed to allocate RGB buffer\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    pkt = av_packet_alloc();

    u8 *rgb_planes[4];
    int rgb_linesize[4];
    av_image_fill_arrays(rgb_planes, rgb_linesize, rgb_data, AV_PIX_FMT_RGB24, width, height * MAX_FRAMES, 1);

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
                fprintf(stderr, "Error sending packet for decoding\n");
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if (ret < 0)
                {
                    fprintf(stderr, "Error during decoding\n");
                    goto cleanup;
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
    output->num_frames = frame_count;
    output->data = rgb_data;

    // Cleanup
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    return true;

cleanup:
    free(rgb_data);
    sws_freeContext(sws_ctx);
    av_frame_free(&frame);
    av_frame_free(&rgb_frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return false;
}
