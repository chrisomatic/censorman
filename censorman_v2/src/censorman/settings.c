
Settings settings_default()
{
    Settings settings = {0};

    settings.detect_types[0] = DETECT_TYPE_FACE;
    settings.detect_type_count = 1;

    settings.filters[0].type = FILTER_TYPE_BLUR_GAUSSIAN;
    settings.filters[0].blur_strength = 0.6;
    settings.filter_count = 1;

    settings.output_folder = S("output");

    settings.thread_count         = 8;
    settings.buffer_size          = MB(512);
    settings.nms_threshold        = 0.45;
    settings.confidence_threshold = 0.25;
    settings.box_padding          = 0.15;
    settings.smoothing_window     = 0.24; // 240ms

    settings.no_encode = false;
    settings.no_rotate = false;
    settings.debug     = false;
    settings.verbose   = false;

    return settings;
}

Settings settings_parse_cmd_line(int argc, char **args)
{
    Settings settings = settings_default();

    Temp scratch = scratch_begin();

    CmdLine cmdline = cmdline_parse(scratch.arena, argc, args);

    String str_assets           = cmdline_get_unflagged(&cmdline, 0);
    String str_detect_types     = cmdline_get_value(&cmdline, S("d"));
    String str_filters          = cmdline_get_value(&cmdline, S("f"));
    String str_output_folder    = cmdline_get_value(&cmdline, S("o"));
    String str_confidence       = cmdline_get_value(&cmdline, S("c"));
    String str_thread_count     = cmdline_get_value(&cmdline, S("thread_count"));
    String str_buffer_size      = cmdline_get_value(&cmdline, S("buffer_size"));
    String str_nms_threshold    = cmdline_get_value(&cmdline, S("nms_threshold"));
    String str_box_padding      = cmdline_get_value(&cmdline, S("box_padding"));
    String str_smoothing_window = cmdline_get_value(&cmdline, S("smoothing_window"));
    String str_texture_path     = cmdline_get_value(&cmdline, S("texture"));
    String str_block_scale      = cmdline_get_value(&cmdline, S("block_scale"));
    String str_blur_strength    = cmdline_get_value(&cmdline, S("blur_strength"));
    String str_bbx_output       = cmdline_get_value(&cmdline, S("bbx_output"));

    StringArray strs_asset        = string_split(scratch.arena, str_assets, S(","));
    StringArray strs_detect_types = string_split(scratch.arena, str_detect_types, S(","));
    StringArray strs_filters      = string_split(scratch.arena, str_filters, S(","));

    if(str_output_folder.len > 0)    settings.output_folder        = str_output_folder;
    if(str_confidence.len > 0)       settings.confidence_threshold = string_to_f64(str_confidence);
    if(str_nms_threshold.len > 0)    settings.nms_threshold        = string_to_f64(str_nms_threshold);
    if(str_box_padding.len > 0)      settings.box_padding          = string_to_f64(str_box_padding);
    if(str_smoothing_window.len > 0) settings.smoothing_window     = string_to_f64(str_smoothing_window);
    if(str_thread_count.len > 0)     settings.thread_count         = string_to_s64(str_thread_count);
    if(str_buffer_size.len > 0)      settings.buffer_size          = string_to_s64(str_buffer_size);

    b32 f_no_encode = cmdline_has_flag(&cmdline,  S("no_encode"));
    b32 f_debug     = cmdline_has_flag(&cmdline,  S("debug"));
    b32 f_verbose   = cmdline_has_flag(&cmdline,  S("verbose"));

    if(f_no_encode) settings.no_encode = true;
    if(f_debug)     settings.debug = true;
    if(f_verbose)   settings.verbose = true;

    scratch_end(scratch);

    return settings;
}

void settings_print(Settings *settings)
{
    logi("");
    logi("=============== Settings ===============");
    logi("  Assets (%d):", settings->asset_count);
    for(int i = 0 ; i < settings->asset_count; ++i)
    {
        Asset *asset = &settings->assets[i];
        logi("   %d: [%d] " STR_FMT, i, asset->type, STR_ARG(asset->path));
    }

    logi("  Detect Types (%d):", settings->detect_type_count);
    for(int i = 0 ; i < settings->detect_type_count; ++i)
    {
        DetectType *detect_type = &settings->detect_types[i];
        logi("   %d: %d", i, *detect_type);
    }

    logi("  Filters (%d):", settings->filter_count);
    for(int i = 0 ; i < settings->filter_count; ++i)
    {
        Filter *filter = &settings->filters[i];
        logi("    %d: %d", i, filter->type);

        switch(filter->type)
        {
            case FILTER_TYPE_BLUR_BOX:
            case FILTER_TYPE_BLUR_GAUSSIAN:
                logi("   Blur Strength: %f", filter->blur_strength);
                break;
            case FILTER_TYPE_PIXELATE:
                logi("   Block Scale: %f", filter->block_scale);
                break;
            case FILTER_TYPE_TEXTURE:
                logi("   Texture Path: " STR_FMT, STR_ARG(filter->texture_path));
                break;
            case FILTER_TYPE_BLACKOUT:
            case FILTER_TYPE_NONE:
            default:
                break;
        }
    }

    logi("  Thread Count:           %u", settings->thread_count);
    logi("  Confidence Threshold:   %f", settings->confidence_threshold);
    logi("  Buffer Size:            %lu B", settings->buffer_size);
    logi("  Box Padding:            %f", settings->box_padding);
    logi("  Smoothing Window:       %f", settings->smoothing_window);
    logi("  No Encoding:            %s", STR_BOOL(settings->no_encode));
    logi("  No Rotate:              %s", STR_BOOL(settings->no_rotate));
    logi("  Bounding Box Output:    " STR_FMT, STR_ARG(settings->bbx_output));
    logi("  Debug:                  %s", settings->debug ? "ON" : "OFF");
    logi("  Verbose:                %s", settings->verbose ? "ON" : "OFF");
    logi("========================================");
}
