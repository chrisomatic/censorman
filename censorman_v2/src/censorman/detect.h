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
    u32 x;
    u32 y;
} Point;

typedef struct
{
    u32 x;
    u32 y;
    u32 w;
    u32 h;
    u16 confidence;
    Point landmarks[5];
    DetectType type;
} Box;

typedef struct
{
    u32 frame_number;
    u32 box_count;
    Box *boxes;
} BoxFrame;

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
    u32 net_w;
    u32 net_h;
} Model;

String detect_type_to_string(DetectType type);
DetectType detect_type_from_string(String str);

BoxFrame convert_list_to_box_frame(Arena *arena, List box_list, u32 frame_number);
void detect_interpolate_boxes(Video *vid, BoxFrame *box_frames);

List detect_faces(Arena *arena, Image *image);
List detect_persons(Arena *arena, Image *image);

b32 detect_init(void);
void *detect(void *args); // thread compatible

void box_print(Box *b);

