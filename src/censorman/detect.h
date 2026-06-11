#pragma once

#include "ncnn/c_api.h"

#define LANDMARK_COUNT 5

typedef enum
{
    DETECT_TYPE_NONE = 0,
    DETECT_TYPE_FACE,
    DETECT_TYPE_FACE_10G,
    DETECT_TYPE_PERSON,
    DETECT_TYPE_LICENSE_PLATE,
    DETECT_TYPE_NUDITY,

    // subtypes used for labels
    DETECT_TYPE_EYE = 16,
    DETECT_TYPE_NOSE,
    DETECT_TYPE_MOUTH,
    DETECT_TYPE_CHEEK,
    DETECT_TYPE_FOREHEAD,
} DetectType;

typedef enum
{
    MODEL_FACE_SCRFD_2_5G = 0,
    MODEL_FACE_SCRFD_10G  = 1,
} ModelFace;

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
    u8  confidence;
    Point landmarks[LANDMARK_COUNT];
    DetectType type;
} Box;

typedef struct
{
    u32 box_count;
    Box *boxes;

    u32 frame_number;
    b32 interpolated;
    b32 detections_run;
} BoxFrame;

typedef struct
{
    DetectType type;

    f32 threshold_confidence;
    f32 threshold_nms;

} DetectConfig;

typedef struct
{
    ncnn_net_t net;
    b32 initialized;
    u32 net_w;
    u32 net_h;
} Model;

// These Report structs are used for 
// diagnostics printing to help tune
// the model and configuration
typedef struct
{
    u64 frame_count_total;
    f32 frames_per_second;

    u64 total_detect_boxes;
    u64 sum_box_width;
    u64 sum_box_height;
    f32 sum_confidence;
    f32 highest_confidence;
} DetectReportItem;

typedef struct
{
    b32 enabled;
    DetectType detect_type;

    s64 item_count;
    DetectReportItem *items;
} DetectReport;

String detect_type_to_string(DetectType type);
DetectType detect_type_from_string(String str);

String detect_model_to_string(ModelFace model);
ModelFace detect_model_from_string(String str);

void detect_interpolate_boxes(Video *vid, BoxFrame *box_frames);
Model detect_get_model_by_type(DetectType type);

List detect_faces(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms);
List detect_faces_retina(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms);
List detect_persons(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms);
List detect_license_plates(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms);
List detect_nudity(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms);

b32 detect_init(DetectConfig *detect_cfgs, s64 config_count);
void detect(DetectConfig *cfg, Image *image, List *total_boxes);

BoxFrame box_frame_from_list(Arena *arena, List box_list, u32 frame_number);
BoxFrame box_frame_divide_into_features(Arena *arena, BoxFrame input, ImageProps *props, u8 facial_features);
void     box_frame_apply_padding(BoxFrame input, ImageProps *props, f32 padding_percent);

Box box_unscale(Box box, Image *image);
Box box_rotate(Box box, ImageProps *props, ClockDir dir);
Box box_pad(Box box, ImageProps *props, f32 padding_percent);
Box box_clamp(Box box, ImageProps *props);

void box_print(Box *b, LogLevel ll);

// Detect Report API
DetectReport detect_report_create(Arena *arena, s64 item_count);
void detect_report_init_item(DetectReport *report, s64 item_index, u64 frame_count_total, f32 fps);
void detect_report_update_item(DetectReport *report, s64 item_index, BoxFrame *box_frame);
void detect_report_print(DetectReport *report, void *settings_v, LogLevel ll);
