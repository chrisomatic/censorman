#pragma once

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/imgutils.h"
#include "libavutil/display.h"
#include "libavutil/dict.h"

typedef struct
{
    s32  stream_index;

    // decoding 

    AVFormatContext   *fmt_ctx;
    AVCodecContext    *codec_ctx;
    const AVCodec     *codec;
    struct SwsContext *sws_ctx;
    AVFrame           *frame;
    AVFrame           *rgb_frame;
    AVPacket          *pkt;
    s32               codec_id;

    // encoding

    AVFormatContext   *enc_fmt_ctx;
    AVCodecContext    *enc_codec_ctx;
    const AVCodec     *enc_codec;
    struct SwsContext *enc_sws_ctx;
    AVFrame           *enc_frame_src;
    AVFrame           *enc_frame;
    AVPacket          *enc_pkt;
    AVStream          *enc_stream;
    AVDictionary      *enc_opts;

    // audio
    s32        audio_stream_index;
    AVStream  *enc_audio_stream;
    AVPacket **audio_packets;
    u32        audio_packet_count;
    u32        audio_packet_max;

    // audio distortion
    AVCodecContext    *audio_dec_ctx;
    const AVCodec     *audio_dec_codec;
    AVCodecContext    *audio_enc_ctx;
    const AVCodec     *audio_enc_codec;
    AVFrame           *audio_frame;
    AVPacket          *audio_enc_pkt;
    SwrContext        *swr_ctx;

} VideoContext;

typedef struct
{
    String output_path;
    u64 max_buffer_size;
    f32 distort_audio_carrier_hz;
    b8  distort_audio;
    b8  no_encode;
    b8  thumbnail;
} VideoSettings;

typedef struct
{
    u32 w;
    u32 h;

    f32 fps;
    s32 rotation;

    RGBColor* data;        // RGB buffer for current chunk
    s64* pts_buffer;       // PTS for each frame in current chunk

    u32 frame_count_max;   // Maximum frames the buffer can hold
    u32 frame_count;       // Number of frames currently in the buffer
    u64 frame_count_total; // Total number of frames in the video
    u32 frames_processed;  // Used during encoding
    b32 load_complete;     // Set when frame decoding is done
    u32 thumbnail_frame;   // frame to take thumbnail of

    VideoContext context;  // Used by FFMPEG

    VideoSettings settings;

    Arena *arena;
} Video;

Video video_nil(void);
b32   video_is_empty(Video *vid);

Video video_begin(Arena *arena, String path, String out_path, VideoSettings *settings);
void  video_end(Video *vid);

ListArray video_get_detect_frames(Video *vid, f32 smoothing_window);

b32       video_load_frames(Video *vid);
b32       video_save_frames(Video *vid);
void      video_save_done(Video *vid);

void video_print(Video *vid);
void video_set_log_level(LogLevel level);
