#pragma once

typedef unsigned char  U8
typedef unsigned short U16
typedef unsigned int   U32
typedef unsigned long  U64

typedef signed char  S8
typedef signed short S16
typedef signed int   S32
typedef signed long  S64

typedef struct
{
    U16 x;
    U16 y;
    U16 w;
    U16 h;
    U16 confidence;
} Rect;

typedef struct
{
    U8 *data;
    U32 w;
    U32 h;
    U32 channels;
} Image;

typedef struct
{
    Image *image;
    U32 x_offset;
    U32 y_offset;
    U32 step;
} DetectInfo;
