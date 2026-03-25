#pragma once

#define ASSET_MAX  1024

typedef enum
{
    TYPE_UNSUPPORTED = 0,
    TYPE_IMAGE = 1,
    TYPE_VIDEO = 2,
} AssetType;

typedef struct
{
    AssetType type;

    String path;
    String output_path;
} Asset;

typedef struct
{
    Asset assets[ASSET_MAX];
    u64 asset_count;

    DetectConfig detect_configs[DETECT_TYPE_MAX];
    u32 detect_config_count;

    Filter filters[FILTER_TYPE_MAX];
    u32 filter_count;
    u8 filter_features; // bit-wise (e.g. eyes, nose, mouth)

    String output_folder;
    String bbx_output;

    u32 thread_count;
    u64 buffer_size;

    f32 nms_threshold;        // [0.0 - 1.0]
    f32 box_padding;          // [0.0 - 1.0]
    f32 smoothing_window;     // [0.0 - 1.0]
    f32 blur_strength;        // [0.0 - 1.0]
    f32 block_scale;          // [0.0 - 1.0]
    f32 distort_audio_carrier_hz;

    // flags
    b8 no_encode;
    b8 no_rotate;
    b8 debug;
    b8 verbose;
    b8 quiet;
    b8 distort_audio;
    b8 help;

} Settings;

AssetType asset_type_from_string(String str);
String asset_type_to_string(AssetType type);

Settings settings_default(void);
Settings settings_parse(Arena *arena, int argc, char **args);
void     settings_print(Settings *settings);
void     settings_print_help(void);
