#pragma once

#define ASSET_MAX  1024

typedef enum
{
    TYPE_UNSUPPORTED = 0,
    TYPE_IMAGE,
    TYPE_VIDEO,
} AssetType;

typedef struct
{
    AssetType type;

    String    path;
    String    output_path;
} Asset;

typedef struct
{
    Asset assets[ASSET_MAX];
    u64 asset_count;

    DetectType detect_types[DETECT_TYPE_MAX];
    u32 detect_type_count;

    Filter filters[FILTER_TYPE_MAX];
    u32 filter_count;

    String output_folder;
    String bbx_output;

    u32 thread_count;
    u64 buffer_size;

    f32 nms_threshold;
    f32 confidence_threshold;
    f32 box_padding;
    f32 smoothing_window; // [0.0 - 1.0]
    f32 blur_strength;    // [0.0 - 1.0]
    f32 block_scale;      // [0.0 - 1.0]

    // flags
    b8 no_encode;
    b8 no_rotate;
    b8 debug;
    b8 verbose;
    b8 quiet;

} Settings;

AssetType asset_from_string(String str);
String asset_to_string(AssetType type);

Settings settings_default();

Settings settings_parse(Arena *arena, int argc, char **args);
void settings_print(Settings *settings);
