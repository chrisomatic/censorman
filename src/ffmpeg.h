extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/display.h>
#include <libavutil/dict.h>
}

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
    s32               video_stream_index;

    // encoding

    AVFormatContext   *enc_fmt_ctx;
    AVCodecContext    *enc_codec_ctx;
    const AVCodec     *enc_codec;
    struct SwsContext *enc_sws_ctx;
    AVFrame           *enc_frame_src;
    AVFrame           *enc_frame;
    AVPacket          *enc_pkt;
    AVStream          *enc_video_st;
    AVDictionary      *enc_opts;

} VideoCtx;

s32 _get_rotation(AVStream *st);

b32 ffmpeg_open(Arena *arena, String filename, const char *outfile, Video *video, VideoCtx *vid_ctx)
{
    //
    // Initialize video context data
    //

    MemoryZeroStruct(vid_ctx);

    if(is_quiet)
    {
        av_log_set_level(AV_LOG_QUIET);
    }
    else
    {
        switch(log_level)
        {
            case LOG_TYPE_ERROR:   av_log_set_level(AV_LOG_ERROR); break;
            case LOG_TYPE_WARNING: av_log_set_level(AV_LOG_WARNING); break;
            case LOG_TYPE_INFO:    av_log_set_level(AV_LOG_INFO); break;
            case LOG_TYPE_VERBOSE: av_log_set_level(AV_LOG_VERBOSE); break;
            default: av_log_set_level(AV_LOG_INFO); break;
        }
    }

    //
    // Set up decoding
    //

    vid_ctx->video_stream_index = -1;

    char *filename_cstr = string_to_cstr(arena, filename);
    if(avformat_open_input(&vid_ctx->fmt_ctx, filename_cstr, NULL, NULL) < 0)
    {
        LOGE("Could not open input file '" STR_FMT "'", STR_ARG(filename));
        return false;
    }

    if(avformat_find_stream_info(vid_ctx->fmt_ctx, NULL) < 0)
    {
        LOGE("Could not find stream info");
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    for(s32 i = 0; i < vid_ctx->fmt_ctx->nb_streams; ++i)
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

    if(!fps.num || !fps.den) 
    {
        // fallback
        fps.num = 24;
        fps.den = 1;
    }

    s64 nb_frames = vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index]->nb_frames;
    video->fps = fps.num / (f64)fps.den;
    video->rotation = _get_rotation(st);

    enum AVCodecID codec_id = vid_ctx->fmt_ctx->streams[vid_ctx->video_stream_index]->codecpar->codec_id;

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

    s32 width = vid_ctx->codec_ctx->width;
    s32 height = vid_ctx->codec_ctx->height;
    u64 rgb_stride = (u64)width * 3;
    u64 frame_rgb_size = rgb_stride * height;
    s32 max_frames = floor(settings.max_buffer_size / (f64)frame_rgb_size);

    // Allocate buffer for up to max frames

    u8 *rgb_data = (u8 *)PUSH_ARRAY(arena, u8, (u64)frame_rgb_size * max_frames);
    if(!rgb_data)
    {
        LOGE("Failed to allocate RGB buffer of size %lu", (u64)frame_rgb_size * max_frames);
        avcodec_free_context(&vid_ctx->codec_ctx);
        avformat_close_input(&vid_ctx->fmt_ctx);
        return false;
    }

    vid_ctx->frame = av_frame_alloc();
    vid_ctx->rgb_frame = av_frame_alloc();
    vid_ctx->pkt = av_packet_alloc();

    u8 *rgb_planes[4];
    s32 rgb_linesize[4];
    av_image_fill_arrays(rgb_planes, rgb_linesize, rgb_data, AV_PIX_FMT_RGB24, width, height, 1);

    vid_ctx->sws_ctx = sws_getContext(width, height, vid_ctx->codec_ctx->pix_fmt,
                             width, height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);

    video->w = width;
    video->h = height;
    video->total_frame_count = (nb_frames > 0 ? nb_frames : -1);
    video->data = rgb_data;
    video->data_max_frames = max_frames;

    // allocate PTS buffer
    video->pts_buffer = (s64 *)PUSH_ARRAY(arena, s64, video->data_max_frames);

    // Set up encoding
    // Output format (MP4 / H264)

    if(!settings.no_encoding)
    {
        avformat_alloc_output_context2(&vid_ctx->enc_fmt_ctx, NULL, "mp4", outfile);
        if(!vid_ctx->enc_fmt_ctx)
        {
            LOGE("Could not deduce output format");
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
            LOGE("Encoder not found");
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }

        // Add new video stream
        vid_ctx->enc_video_st = avformat_new_stream(vid_ctx->enc_fmt_ctx, NULL);
        if(!vid_ctx->enc_video_st)
        {
            LOGE("Could not create stream");
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }

        vid_ctx->enc_codec_ctx = avcodec_alloc_context3(vid_ctx->enc_codec);
        if(!vid_ctx->enc_codec_ctx)
        {
            LOGE("Could not allocate codec context");
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }

        LOGI("Video Details:");
        LOGI("  Size:        %d, %d", width, height);
        LOGI("  Frame count: %ld", nb_frames);
        LOGI("  FPS:         %f", fps.num / (f64)fps.den);
        LOGI("  Rotation:    %d", video->rotation);
        LOGI("  Codec:       %s (%d)", avcodec_get_name(codec_id), codec_id);

        // Basic encoding settings

        vid_ctx->enc_codec_ctx->codec_id = vid_ctx->enc_codec->id;
        vid_ctx->enc_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
        vid_ctx->enc_codec_ctx->width = video->w;
        vid_ctx->enc_codec_ctx->height = video->h;
        vid_ctx->enc_codec_ctx->time_base = av_inv_q(fps);
        vid_ctx->enc_codec_ctx->framerate = fps;
        vid_ctx->enc_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        vid_ctx->enc_codec_ctx->gop_size = 12;
        vid_ctx->enc_codec_ctx->max_b_frames = 0;
        vid_ctx->enc_codec_ctx->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;
        vid_ctx->enc_codec_ctx->thread_count = 0; // auto

        vid_ctx->enc_video_st->time_base  = vid_ctx->enc_codec_ctx->time_base;

        if (vid_ctx->enc_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            vid_ctx->enc_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        av_dict_set(&vid_ctx->enc_opts, "preset", "superfast", 0); // ultrafast, superfast, fast, medium, slow, placebo
        av_dict_set(&vid_ctx->enc_opts, "tune", "zerolatency", 0);

        s32 ret;

        // Open encoder
        if((ret = avcodec_open2(vid_ctx->enc_codec_ctx, vid_ctx->enc_codec, &vid_ctx->enc_opts)) < 0)
        {
            LOGE("Could not open encoder");
            avcodec_free_context(&vid_ctx->enc_codec_ctx);
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }

        // Copy codec params to stream
        ret = avcodec_parameters_from_context(vid_ctx->enc_video_st->codecpar, vid_ctx->enc_codec_ctx);
        if(ret < 0)
        {
            LOGE("Could not copy codec parameters");
            avcodec_free_context(&vid_ctx->enc_codec_ctx);
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }
        
        // set rotation on encoder stream
        char rotate[16];
        snprintf(rotate, sizeof(rotate), "%d", video->rotation);
        av_dict_set(&vid_ctx->enc_video_st->metadata, "rotate", rotate, 0);

        // Open output file
        if(!(vid_ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE))
        {
            if(avio_open(&vid_ctx->enc_fmt_ctx->pb, outfile, AVIO_FLAG_WRITE) < 0)
            {
                LOGE("Could not open output file '%s'", outfile);
                avcodec_free_context(&vid_ctx->enc_codec_ctx);
                avformat_free_context(vid_ctx->enc_fmt_ctx);
                return false;
            }
        }

        // Write header
        if(avformat_write_header(vid_ctx->enc_fmt_ctx, NULL) < 0)
        {
            LOGE("Error occurred writing header");
            avio_close(vid_ctx->enc_fmt_ctx->pb);
            avcodec_free_context(&vid_ctx->enc_codec_ctx);
            avformat_free_context(vid_ctx->enc_fmt_ctx);
            return false;
        }

        // Allocate frame + packet
        vid_ctx->enc_frame_src = av_frame_alloc();
        vid_ctx->enc_frame = av_frame_alloc();
        vid_ctx->enc_pkt = av_packet_alloc();

        if(!vid_ctx->enc_frame || !vid_ctx->enc_frame_src || !vid_ctx->enc_pkt)
        {
            LOGE("Could not allocate frame/packet");
            return false;
        }

        vid_ctx->enc_frame_src->format = AV_PIX_FMT_RGB24;
        vid_ctx->enc_frame_src->width  = video->w;
        vid_ctx->enc_frame_src->height = video->h;

        vid_ctx->enc_frame->format = vid_ctx->enc_codec_ctx->pix_fmt;
        vid_ctx->enc_frame->width  = vid_ctx->enc_codec_ctx->width;
        vid_ctx->enc_frame->height = vid_ctx->enc_codec_ctx->height;

        // SWS converter (RGB24 -> YUV420P)
        vid_ctx->enc_sws_ctx = sws_getContext(width, height, AV_PIX_FMT_RGB24,
                                 width, height, vid_ctx->enc_codec_ctx->pix_fmt,
                                 SWS_BILINEAR, NULL, NULL, NULL);

        if(!vid_ctx->enc_sws_ctx)
        {
            LOGE("Could not init sws context");
            return false;
        }

        vid_ctx->enc_frame->pict_type = AV_PICTURE_TYPE_I;
    }

    return true;
}

b32 ffmpeg_decode_ctx(Video *video, VideoCtx *vid_ctx)
{
    if(!video->data) return false;

    s32 width  = vid_ctx->codec_ctx->width;
    s32 height = vid_ctx->codec_ctx->height;
    s32 rgb_stride = width * 3;
    s32 frame_rgb_size = rgb_stride * height;

    u32 frame_count = 0;
    s32 ret;
    b32 eof_reached = false; // track end of file
    b32 hit_max_buffer = false;

    AVFrame *frame = vid_ctx->frame;
    AVPacket *pkt  = vid_ctx->pkt;

    while(frame_count < video->data_max_frames)
    {
        ret = av_read_frame(vid_ctx->fmt_ctx, pkt);
        if(ret < 0) // EOF or error
        {
            eof_reached = true;
            break;
        }

        if(pkt->stream_index != vid_ctx->video_stream_index)
        {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(vid_ctx->codec_ctx, pkt);
        av_packet_unref(pkt);
        if(ret < 0) break;

        while(ret >= 0)
        {
            ret = avcodec_receive_frame(vid_ctx->codec_ctx, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            else if(ret < 0) return false;

            // Convert to RGB
            u8 *dp = video->data + (u64)frame_count * frame_rgb_size;
            u8 *dest_data[4] = { dp, NULL, NULL, NULL };
            s32 dest_linesize[4] = { rgb_stride, 0, 0, 0 };
            sws_scale(vid_ctx->sws_ctx, (const u8 *const*)frame->data, frame->linesize,
                      0, height, dest_data, dest_linesize);

            video->pts_buffer[frame_count] = frame->pts;
            frame_count++;
            if(frame_count >= video->data_max_frames) {
                hit_max_buffer = true;
                break;
            }
        }
    }

    // flush decoder if EOF
    if(eof_reached)
    {
        avcodec_send_packet(vid_ctx->codec_ctx, NULL);
        while(avcodec_receive_frame(vid_ctx->codec_ctx, frame) == 0 && frame_count < video->data_max_frames)
        {
            u8 *dp = video->data + (u64)frame_count * frame_rgb_size;
            u8 *dest_data[4] = { dp, NULL, NULL, NULL };
            s32 dest_linesize[4] = { rgb_stride, 0, 0, 0 };
            sws_scale(vid_ctx->sws_ctx, (const u8 *const*)frame->data, frame->linesize,
                      0, height, dest_data, dest_linesize);

            video->pts_buffer[frame_count] = frame->pts;
            frame_count++;
        }
    }

    video->frame_count = frame_count;
    video->decode_complete = !hit_max_buffer || eof_reached;

    return (frame_count > 0);
}

b32 ffmpeg_encode_ctx(Video *video, VideoCtx *vid_ctx)
{
    if (!video->data || video->frame_count == 0) return false;

    s32 width = video->w;
    s32 height = video->h;
    s32 rgb_stride = width * 3;
    s32 frame_rgb_size = rgb_stride * height;
    s32 ret;

    for (s32 i = 0; i < video->frame_count; ++i)
    {
        av_frame_make_writable(vid_ctx->enc_frame);

        // Source RGB data
        vid_ctx->enc_frame_src->data[0] = video->data + ((u64)i * frame_rgb_size);
        vid_ctx->enc_frame_src->linesize[0] = rgb_stride;

        // Convert RGB -> YUV420P
        ret = sws_scale_frame(vid_ctx->enc_sws_ctx, vid_ctx->enc_frame, vid_ctx->enc_frame_src);
        if(ret < 0)
        {
            LOGW("Error scaling frame %d", i);
            continue;
        }

        // Assign continuous PTS based on chunk index + global frames processed
        // This avoids using potentially huge input PTS and keeps output framerate consistent
        vid_ctx->enc_frame->pts = video->frames_processed + i;

        // Send to encoder
        ret = avcodec_send_frame(vid_ctx->enc_codec_ctx, vid_ctx->enc_frame);
        if(ret < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            LOGE("Error sending frame to encoder: %s", err);
            continue;
        }

        // Receive and write all packets
        while(ret >= 0)
        {
            ret = avcodec_receive_packet(vid_ctx->enc_codec_ctx, vid_ctx->enc_pkt);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if(ret < 0)
            {
                char err[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err, sizeof(err));
                LOGE("Error encoding frame: %s", err);
                break;
            }

            vid_ctx->enc_pkt->stream_index = vid_ctx->enc_video_st->index;

            // Rescale to output stream timebase
            av_packet_rescale_ts(vid_ctx->enc_pkt, vid_ctx->enc_codec_ctx->time_base,
                                 vid_ctx->enc_video_st->time_base);

            av_interleaved_write_frame(vid_ctx->enc_fmt_ctx, vid_ctx->enc_pkt);
            av_packet_unref(vid_ctx->enc_pkt);
        }
    }

    // Update global frames processed for next chunk
    video->frames_processed += video->frame_count;

    return true;
}

b32 ffmpeg_encode_done(VideoCtx *vid_ctx)
{
    s32 ret;

    // Flush encoder: send NULL until encoder returns AVERROR_EOF
    ret = avcodec_send_frame(vid_ctx->enc_codec_ctx, NULL);
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(vid_ctx->enc_codec_ctx, vid_ctx->enc_pkt);
        if (ret == AVERROR(EAGAIN))
            continue;  // keep sending
        else if (ret == AVERROR_EOF)
            break;     // encoder fully flushed
        else if (ret < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            LOGE("Error flushing encoder: %s", err);
            break;
        }

        vid_ctx->enc_pkt->stream_index = vid_ctx->enc_video_st->index;

        // Rescale packet timestamps to output stream timebase
        av_packet_rescale_ts(vid_ctx->enc_pkt, vid_ctx->enc_codec_ctx->time_base,
                             vid_ctx->enc_video_st->time_base);

        av_interleaved_write_frame(vid_ctx->enc_fmt_ctx, vid_ctx->enc_pkt);
        av_packet_unref(vid_ctx->enc_pkt);
    }

    // Write trailer
    av_write_trailer(vid_ctx->enc_fmt_ctx);

    // Optional: flush underlying IO
    if (!(vid_ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE) && vid_ctx->enc_fmt_ctx->pb)
        avio_flush(vid_ctx->enc_fmt_ctx->pb);

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


//:========================
// Helper functions
//:========================

s32 _get_rotation(AVStream *st)
{
    // 1. Try side_data in codecpar
    for (s32 i = 0; i < st->codecpar->nb_coded_side_data; i++) {
        const AVPacketSideData *sd = &st->codecpar->coded_side_data[i];
        if (sd->type == AV_PKT_DATA_DISPLAYMATRIX) {
            if (sd->size >= sizeof(s32) * 9) {
                f64 angle = av_display_rotation_get((const s32*)sd->data);
                // Normalize (FFmpeg returns e.g. 0.000001, — round)
                s32 ang = (s32)round(angle);
                // make sure it's one of 0,90,180,270
                ang = ((ang % 360) + 360) % 360;
                if (ang == 0 || ang == 90 || ang == 180 || ang == 270)
                    return ang;
                // If weird angle, maybe return 0
                return 0;
            }
        }
    }

    // 2. Fallback: metadata “rotate” tag
    AVDictionaryEntry *tag = av_dict_get(st->metadata, "rotate", NULL, 0);
    if (tag && tag->value) {
        s32 ang = atoi(tag->value);
        ang = ((ang % 360) + 360) % 360;
        if (ang == 0 || ang == 90 || ang == 180 || ang == 270)
            return ang;
    }

    // 3. Default: no rotation
    return 0;
}
