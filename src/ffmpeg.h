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

typedef struct
{
    // decoding 

    AVFormatContext   *fmt_ctx;
    AVCodecContext    *codec_ctx;
    const AVCodec     *codec;
    struct SwsContext *sws_ctx;
    AVFrame           *frame;
    AVFrame           *rgb_frame;
    AVPacket          *pkt;
    int               video_stream_index;

    // encoding

    AVFormatContext   *enc_fmt_ctx;
    AVCodecContext    *enc_codec_ctx;
    const AVCodec     *enc_codec;
    struct SwsContext *enc_sws_ctx;
    AVFrame           *enc_frame;
    AVPacket          *enc_pkt;
    AVStream          *enc_video_st;
    AVDictionary      *enc_opts;

} VideoCtx;

bool ffmpeg_open(const char *filename, const char *outfile, Video *video, VideoCtx *vid_ctx)
{
    //
    // Initialize video context data
    //

    MemoryZeroStruct(vid_ctx);

    //
    // Set up decoding
    //

    vid_ctx->video_stream_index = -1;

    if(avformat_open_input(&vid_ctx->fmt_ctx, filename, NULL, NULL) < 0)
    {
        LOGE("Could not open input file '%s'", filename);
        return false;
    }

    if(avformat_find_stream_info(vid_ctx->fmt_ctx, NULL) < 0)
    {
        LOGE("Could not find stream info");
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    for(int i = 0; i < vid_ctx->fmt_ctx->nb_streams; ++i)
    {
        if (vid_ctx->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            vid_ctx->video_stream_index = i;
            break;
        }
    }

    if(vid_ctx->video_stream_index == -1)
    {
        LOGE("No video stream found");
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    AVStream *st = vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index];
    AVRational fps = st->avg_frame_rate.num ? st->avg_frame_rate : st->r_frame_rate;
    if (!fps.num || !fps.den) fps = (AVRational){24,1}; // fallback

    int64_t nb_frames = vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index]->nb_frames;
    LOGI("Num frames: %ld", nb_frames);

    video->seconds_per_frame = fps.num / (double)fps.den;

    enum AVCodecID codec_id = vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index]->codecpar->codec_id;
    LOGI("Video codec id: %d (%s)", codec_id, avcodec_get_name(codec_id));

    vid_ctx->codec = avcodec_find_decoder(vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index]->codecpar->codec_id);
    if(!vid_ctx->codec)
    {
        LOGE("Unsupported codec");
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    vid_ctx->codec_ctx = avcodec_alloc_context3(vid_ctx->codec);
    if(!vid_ctx->codec_ctx)
    {
        LOGE("Could not allocate codec context");
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    avcodec_parameters_to_context(vid_ctx->codec_ctx, vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index]->codecpar);

    // Enable internal multithreading
    vid_ctx->codec_ctx->thread_count = 0;  // 0 = auto
    vid_ctx->codec_ctx->thread_type = FF_THREAD_FRAME; // or FF_THREAD_SLICE

    if (avcodec_open2(vid_ctx->codec_ctx, vid_ctx->codec, NULL) < 0)
    {
        LOGE("Could not open codec");
        avcodec_free_context(&vid_ctx->codec_ctx);
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    int width = vid_ctx->codec_ctx->width;
    int height = vid_ctx->codec_ctx->height;
    int rgb_stride = width * 3;
    int frame_rgb_size = rgb_stride * height;

    // Allocate buffer for up to MAX_FRAMES
    printf("frame rgb size: %d\n", frame_rgb_size);
    u8 *rgb_data = (u8 *)malloc((u64)frame_rgb_size * MAX_FRAMES);
    if(!rgb_data)
    {
        LOGE("Failed to allocate RGB buffer of size %lu", (u64)frame_rgb_size * MAX_FRAMES);
        avcodec_free_context(&vid_ctx->codec_ctx);
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    vid_ctx->frame = av_frame_alloc();
    vid_ctx->rgb_frame = av_frame_alloc();
    vid_ctx->pkt = av_packet_alloc();

    u8 *rgb_planes[4];
    int rgb_linesize[4];
    av_image_fill_arrays(rgb_planes, rgb_linesize, rgb_data, AV_PIX_FMT_RGB24, width, height, 1);

    vid_ctx->sws_ctx = sws_getContext(width, height, vid_ctx->codec_ctx->pix_fmt,
                             width, height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);

    video->w = width;
    video->h = height;
    video->total_frame_count = (nb_frames > 0 ? nb_frames : -1);
    video->data = rgb_data;

    // Set up encoding
    // Output format (MP4 / H264)

    avformat_alloc_output_context2(&vid_ctx->enc_fmt_ctx, NULL, "mp4", outfile);
    if(!vid_ctx->enc_fmt_ctx)
    {
        fprintf(stderr, "Could not deduce output format\n");
        return false;
    }

    vid_ctx->enc_codec = avcodec_find_encoder_by_name("libx264");

    if(!vid_ctx->enc_codec)
    {
        vid_ctx->enc_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    if(!vid_ctx->enc_codec)
    {
        vid_ctx->enc_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    }

    if(!vid_ctx->enc_codec)
    {
        fprintf(stderr, "Encoder not found\n");
        avformat_free_context(vid_ctx->enc_fmt_ctx);
        return false;
    }

    // Add new video stream
    vid_ctx->enc_video_st = avformat_new_stream(vid_ctx->enc_fmt_ctx, NULL);
    if(!vid_ctx->enc_video_st)
    {
        fprintf(stderr, "Could not create stream\n");
        avformat_free_context(vid_ctx->enc_fmt_ctx);
        return false;
    }

    vid_ctx->enc_codec_ctx = avcodec_alloc_context3(vid_ctx->enc_codec);
    if(!vid_ctx->enc_codec_ctx)
    {
        fprintf(stderr, "Could not allocate codec context\n");
        avformat_free_context(vid_ctx->enc_fmt_ctx);
        return false;
    }

    // Basic encoding settings
    printf("FPS: %f\n", fps.num / (double)fps.den);

    vid_ctx->enc_codec_ctx->codec_id = vid_ctx->enc_codec->id;
    vid_ctx->enc_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    vid_ctx->enc_codec_ctx->width = width;
    vid_ctx->enc_codec_ctx->height = height;
    vid_ctx->enc_codec_ctx->time_base = av_inv_q(fps);   // 1/fps
    vid_ctx->enc_codec_ctx->framerate = fps;
    vid_ctx->enc_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;      // encoder wants YUV420P
    vid_ctx->enc_codec_ctx->gop_size = 12;
    vid_ctx->enc_codec_ctx->max_b_frames = 0;
    vid_ctx->enc_codec_ctx->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;
    vid_ctx->enc_codec_ctx->thread_count = 0; // auto

    vid_ctx->enc_video_st->time_base  = vid_ctx->enc_codec_ctx->time_base;

    if (vid_ctx->enc_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        vid_ctx->enc_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_dict_set(&vid_ctx->enc_opts, "preset", "superfast", 0); // ultrafast, superfast, fast, medium, slow, placebo
    av_dict_set(&vid_ctx->enc_opts, "tune", "zerolatency", 0);

    int ret;

    // Open encoder
    if((ret = avcodec_open2(vid_ctx->enc_codec_ctx, vid_ctx->enc_codec, &vid_ctx->enc_opts)) < 0)
    {
        fprintf(stderr, "Could not open encoder\n");
        avcodec_free_context(&vid_ctx->enc_codec_ctx);
        avformat_free_context(vid_ctx->enc_fmt_ctx);
        return false;
    }

    // Copy codec params to stream
    ret = avcodec_parameters_from_context(vid_ctx->enc_video_st->codecpar, vid_ctx->enc_codec_ctx);
    if(ret < 0)
    {
        fprintf(stderr, "Could not copy codec parameters\n");
        avcodec_free_context(&vid_ctx->enc_codec_ctx);
        avformat_free_context(vid_ctx->enc_fmt_ctx);
        return false;
    }

    // Open output file
    if(!(vid_ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if(avio_open(&vid_ctx->enc_fmt_ctx->pb, outfile, AVIO_FLAG_WRITE) < 0)
        {
            fprintf(stderr, "Could not open output file '%s'\n", outfile);
            avcodec_free_context(&vid_ctx->enc_codec_ctx);
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }
    }

    // Write header
    if(avformat_write_header(vid_ctx->enc_fmt_ctx, NULL) < 0)
    {
        fprintf(stderr, "Error occurred writing header\n");
        avio_close(vid_ctx->enc_fmt_ctx->pb);
        avcodec_free_context(&vid_ctx->enc_codec_ctx);
        avformat_free_context(vid_ctx->enc_fmt_ctx);
        return false;
    }

    // Allocate frame + packet
    vid_ctx->enc_frame = av_frame_alloc();
    vid_ctx->enc_pkt = av_packet_alloc();
    if(!vid_ctx->enc_frame || !vid_ctx->enc_pkt)
    {
        fprintf(stderr, "Could not allocate frame/packet\n");
        return false;
    }

    vid_ctx->enc_frame->format = vid_ctx->enc_codec_ctx->pix_fmt;
    vid_ctx->enc_frame->width  = vid_ctx->enc_codec_ctx->width;
    vid_ctx->enc_frame->height = vid_ctx->enc_codec_ctx->height;

    if(av_frame_get_buffer(vid_ctx->enc_frame, 32) < 0)
    {
        fprintf(stderr, "Could not allocate frame buffer\n");
        return false;
    }

    // SWS converter (RGB24 -> YUV420P)
    vid_ctx->enc_sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                             width, height, vid_ctx->enc_codec_ctx->pix_fmt,
                             SWS_BILINEAR, NULL, NULL, NULL);

    if(!vid_ctx->enc_sws_ctx)
    {
        fprintf(stderr, "Could not init sws context\n");
        return false;
    }

    vid_ctx->enc_frame->pict_type = AV_PICTURE_TYPE_I;

    return true;
}

bool ffmpeg_decode_ctx(Video *video, VideoCtx *vid_ctx)
{
    u32 frame_count = 0;
    int ret;

    if(!video->data)
    {
        return false;
    }

    int width = vid_ctx->codec_ctx->width;
    int height = vid_ctx->codec_ctx->height;

    int rgb_stride = width * 3;
    int frame_rgb_size = rgb_stride * height;

    bool full_decode = true;

    while(av_read_frame(vid_ctx->fmt_ctx, vid_ctx->pkt) >= 0)
    {
        if(frame_count >= MAX_FRAMES)
        {
            full_decode = false;
            break;
        }

        if(vid_ctx->pkt->stream_index == vid_ctx->video_stream_index)
        {
            ret = avcodec_send_packet(vid_ctx->codec_ctx, vid_ctx->pkt);
            if(ret < 0)
            {
                LOGE("Error sending packet for decoding");
                break;
            }

            while(ret >= 0)
            {
                ret = avcodec_receive_frame(vid_ctx->codec_ctx, vid_ctx->frame);
                if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                else if(ret < 0)
                {
                    LOGE("Error during decoding");
                    return false;
                }

                // Convert frame to RGB
                u8 *dp = video->data + (u64)frame_count * frame_rgb_size;
                u8 *dest_data[4] = { dp, NULL, NULL, NULL };

                int dest_linesize[4] = { rgb_stride, 0, 0, 0 };
                sws_scale(vid_ctx->sws_ctx, (const u8 *const *)vid_ctx->frame->data, vid_ctx->frame->linesize, 0, height, dest_data, dest_linesize);

                frame_count++;
                if (frame_count >= MAX_FRAMES)
                    break;
            }
        }
        av_packet_unref(vid_ctx->pkt);
    }

    // Fill the output structure
    video->frame_count = frame_count;
    video->decode_complete = full_decode;
    video->total_frame_count = full_decode ? (i64)frame_count : video->total_frame_count;

    return true;
}

bool ffmpeg_encode_ctx(Video *video, VideoCtx *vid_ctx)
{
    int ret;
    int rgb_stride = video->w * 3;

    LOGI("Encoding video, frame_count: %d, video size: %d %d\n", video->frame_count, video->w, video->h);

    for (int i = 0; i < video->frame_count; ++i)
    {
        const u8 *rgb_data[1] = { video->data + ((u64)i * rgb_stride * video->h)};
        int rgb_linesize[1] = { rgb_stride };

        av_frame_make_writable(vid_ctx->enc_frame);
        sws_scale(vid_ctx->enc_sws_ctx, rgb_data, rgb_linesize, 0, video->h, vid_ctx->enc_frame->data, vid_ctx->enc_frame->linesize);

        vid_ctx->enc_frame->pts = video->frames_processed++;

        // Encode
        ret = avcodec_send_frame(vid_ctx->enc_codec_ctx, vid_ctx->enc_frame);
        if(ret < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            fprintf(stderr, "Error sending frame: %s\n", err);
            continue;
        }

        while(ret >= 0)
        {
            ret = avcodec_receive_packet(vid_ctx->enc_codec_ctx, vid_ctx->enc_pkt);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                fprintf(stderr, "Error encoding frame\n");
                break;
            }

            vid_ctx->enc_pkt->stream_index = vid_ctx->enc_video_st->index;
            av_packet_rescale_ts(vid_ctx->enc_pkt, vid_ctx->enc_codec_ctx->time_base, vid_ctx->enc_video_st->time_base);
            av_interleaved_write_frame(vid_ctx->enc_fmt_ctx, vid_ctx->enc_pkt);
            av_packet_unref(vid_ctx->enc_pkt);
        }
    }
    

    return true;
}

bool ffmpeg_encode_done(VideoCtx *vid_ctx)
{
    // Flush encoder
    avcodec_send_frame(vid_ctx->enc_codec_ctx, NULL);
    while(avcodec_receive_packet(vid_ctx->enc_codec_ctx, vid_ctx->enc_pkt) == 0)
    {
        vid_ctx->enc_pkt->stream_index = vid_ctx->enc_video_st->index;
        av_packet_rescale_ts(vid_ctx->enc_pkt, vid_ctx->enc_codec_ctx->time_base, vid_ctx->enc_video_st->time_base);
        av_interleaved_write_frame(vid_ctx->enc_fmt_ctx, vid_ctx->enc_pkt);
        av_packet_unref(vid_ctx->enc_pkt);
    }

    // Write trailer
    av_write_trailer(vid_ctx->enc_fmt_ctx);

    return true;
}

void ffmpeg_close(VideoCtx *ctx)
{
    if(ctx->codec_ctx)  avcodec_free_context(&ctx->codec_ctx);
    //if(ctx->codec)    
    if(ctx->frame)      av_frame_free(&ctx->frame);
    if(ctx->rgb_frame)  av_frame_free(&ctx->rgb_frame);
    if(ctx->pkt)        av_packet_free(&ctx->pkt);

    if(ctx->enc_sws_ctx)   sws_freeContext(ctx->enc_sws_ctx);
    if(ctx->enc_frame)     av_frame_free(&ctx->enc_frame);
    if(ctx->enc_pkt)       av_packet_free(&ctx->enc_pkt);
    if(ctx->enc_codec_ctx) avcodec_free_context(&ctx->enc_codec_ctx);

    if(ctx->fmt_ctx)
    {
        if(!(ctx->fmt_ctx->oformat->flags & AVFMT_NOFILE) && ctx->fmt_ctx->pb)
        {
            avio_close(ctx->fmt_ctx->pb);
        }
        avformat_free_context(ctx->fmt_ctx);
    }

    if(ctx->enc_fmt_ctx)
    {
        if(!(ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE) && ctx->enc_fmt_ctx->pb)
        {
            avio_close(ctx->enc_fmt_ctx->pb);
        }
        avformat_free_context(ctx->enc_fmt_ctx);
    }
}
