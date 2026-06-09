#pragma once

typedef enum
{
    ROTATE_0   = 0,
    ROTATE_90  = 90,
    ROTATE_180 = 180,
    ROTATE_270 = 270,
} Rotation;

typedef enum
{
    CW  = 0,
    CCW = 1,
} ClockDir;

typedef struct __attribute__((packed))
{
    u8 r;
    u8 g;
    u8 b;
} RGBColor;

typedef struct
{
    u32 w;
    u32 h;
    s32 pad_x;
    s32 pad_y;
    Rotation rotation;
    f32 scale;
} ImageProps;

typedef struct
{
    RGBColor *data;

    ImageProps props;

    // maintained original properties before
    // image_rotate or image_scale
    ImageProps props_orig;

    Arena *arena;
    Stopwatch *stopwatch;

} Image;

typedef struct
{
    s32  start;
    s32  count;
    f32 *weights;
} LanczosFilter;

Image image_nil(void);
b32 image_is_empty(Image *image);

Image image_load(Arena *arena, String path, Stopwatch *stopwatch);
b32 image_save(Image *image, String path);

Image image_rotate(Image source, u32 degrees, ClockDir direction);
Image image_scale(Image source, u32 target_width, u32 target_height, b32 remove_margins);
Image image_scale_lanczos(Image source, u32 target_width, u32 target_height, u32 a, b32 remove_margins);

Rotation image_get_rotation_from_file(char *file_path);
b32 image_copy_rect(Image *src, s32 src_x, s32 src_y, Image *dst, s32 dst_x, s32 dst_y, s32 width, s32 height);

void image_print(Image *image, LogLevel ll);
