#pragma once

typedef struct
{
    int x;
    int y;
    int w;
    int h;
    int confidence;
} Rect;

typedef struct
{
    unsigned char *data;
    int w;
    int h;
    int channels;
} Image;

