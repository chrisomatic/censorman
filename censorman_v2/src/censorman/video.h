#pragma once

typedef struct
{
    u32 w;
    u32 h;

    f32 fps;
    s32 rotation;

    RGBColor* data;        // RGB buffer for current chunk

    u32 frame_count;       // number of frames currently in the buffer
    s64 frame_count_total; // total number of frames in the video

    Arena *arena;
} Video;

Video video_begin(Arena *arena, String path);

void      video_load_frames(Video *vid, u64 buffer_size);
ListArray video_get_detect_frames(Video *vid, f32 smoothing_window);

void video_end(&vid);
