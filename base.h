#pragma once

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;

typedef signed char  s8;
typedef signed short s16;
typedef signed int   s32;
typedef signed long  s64;

typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u16 confidence;
} Rect;

typedef struct
{
    u8 *data;
    int w;
    int h;
    int channels;
} Image;

typedef struct
{
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} Color;
