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

typedef struct
{
    u8 r;
    u8 g;
    u8 b;
} RGBPixel;

typedef struct
{
    RGBPixel *data;

    u32 w; // width
    u32 h; // height
    u32 n; // channels

    Rotation rotation;

    Arena *arena;
} Image;

Image image_nil();
u32 image_step(Image *image);

Image image_load(Arena *arena, String path);
b32 image_save(Image *image, String path);

Image image_rotate(Image source, u32 degrees, ClockDir direction);
Image image_scale(Image source, u32 target_w, u32 target_h);
