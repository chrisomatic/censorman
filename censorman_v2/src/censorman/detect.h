#pragma once

#include "ncnn/c_api.h"

#define LANDMARK_COUNT 5

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
    s32 x;
    s32 y;
} Point;

typedef struct
{
    s32 x;
    s32 y;
    s32 w;
    s32 h;
    u16 confidence;
    Point landmarks[LANDMARK_COUNT];
    DetectType type;
} Box;

typedef struct
{
    u32 box_count;
    Box *boxes;

    u32 frame_number;
    b32 interpolated;
} BoxFrame;

typedef struct
{
    DetectType type;
    Image *image;
    List *boxes;
    s64 thread_index;
} DetectArgs;

typedef struct
{
    ncnn_net_t *nets;
    b32 initialized;
    u32 net_w;
    u32 net_h;
} Model;

String detect_type_to_string(DetectType type);
DetectType detect_type_from_string(String str);

BoxFrame convert_list_to_box_frame(Arena *arena, List box_list, u32 frame_number);
void detect_interpolate_boxes(Video *vid, BoxFrame *box_frames);

List detect_faces(Arena *arena, Image *image, s64 thread_index);
List detect_persons(Arena *arena, Image *image, s64 thread_index);

b32 detect_init(Arena *arena, s32 thread_count);
void detect(void *args);

Box box_unscale(Box box, Image *image);
Box box_rotate(Box box, Image *image, Rotation rotation, ClockDir dir);

void box_print(Box *b);

