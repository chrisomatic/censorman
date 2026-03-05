#pragma once

#include "ncnn/c_api.h"

typedef enum
{
    DETECT_TYPE_NONE = 0,
    DETECT_TYPE_FACE,
    DETECT_TYPE_PERSON,
    DETECT_TYPE_LICENSE_PLATE,
    DETECT_TYPE_DOCUMENT,
    DETECT_TYPE_MAX,
} DetectType;

typedef struct
{
    u16 x;
    u16 y;
} Point;

typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u16 confidence;
    Point landmarks[5];
} Box;

typedef struct
{
    DetectType type;
    Image *image;
    List *boxes;
} DetectArgs;

typedef struct
{
    ncnn_net_t net;
    b32 initialized;
    u16 net_w;
    u16 net_h;
} Model;

String detect_type_to_string(DetectType type);
DetectType detect_type_from_string(String str);

List detect_faces(Arena *arena, Image *image);
List detect_persons(Arena *arena, Image *image);

b32 detect_init(void);
void *detect(void *args); // thread compatible

void box_print(Box *b);

