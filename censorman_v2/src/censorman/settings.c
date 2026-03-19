
/*
 
USAGE

    censorman <asset_path> [options]

ASSET_PATH

    Accespt 

OPTIONS

    --filters [-f] <filters>
        a comma-separated list of filters [blur_gaussian, blur, pixelate, scramble, blackout]
        default: blur

    --detect [-d] <detect-types>
        default: face
    
EXAMPLES

    # Detect faces in test1.jpg and pixelate with a block scale of 0.12
    censorman assets/images/test1.jpg -d face -f pixelate:0.12
   
    censorman assets/videos/vid1.mp4 -d face -f blur --debug

*/

void settings_print_help()
{
    // @TODO
    fprintf(stdout, "Help!\n");
}

Settings settings_default()
{
    Settings settings = {0};

    settings.detect_types[0] = DETECT_TYPE_FACE;
    settings.detect_type_count = 1;

    settings.filters[0].type = FILTER_TYPE_BLUR_BOX;
    settings.filters[0].blur_strength = 0.6;
    settings.filter_count = 1;

    settings.output_folder = S("output");

    settings.thread_count         = os_system_info.logical_processor_count;
    settings.buffer_size          = MB(512);
    settings.nms_threshold        = 0.45;
    settings.confidence_threshold = 0.25;
    settings.box_padding          = 0.15;
    settings.blur_strength        = 0.60;
    settings.block_scale          = 0.12;
    settings.smoothing_window     = 0.200; // 200ms

    settings.no_encode = false;
    settings.no_rotate = false;
    settings.debug     = false;
    settings.verbose   = false;
    settings.quiet     = false;
    settings.help      = false;

    return settings;
}

Settings settings_parse(Arena *arena, int argc, char **args)
{
    Settings settings = settings_default();

    Temp scratch = scratch_begin();

    CmdLine cmdline = cmdline_parse(scratch.arena, argc, args);

    StringArray ids_help             = string_array_create(scratch.arena, 3, S("help"),             S("?"),      S("h"));
    StringArray ids_no_encode        = string_array_create(scratch.arena, 3, S("no_encode"),        S("ne"),     S("z"));
    StringArray ids_quiet            = string_array_create(scratch.arena, 3, S("quiet"),            S("shh"),    S("q"));
    StringArray ids_debug            = string_array_create(scratch.arena, 3, S("debug"),            S("db") ,    S("g"));
    StringArray ids_verbose          = string_array_create(scratch.arena, 3, S("verbose"),          S("vb") ,    S("v"));
    StringArray ids_distort_audio    = string_array_create(scratch.arena, 3, S("distort_audio"),    S("distort"),S("a"));
    StringArray ids_detect_types     = string_array_create(scratch.arena, 3, S("detect_types"),     S("detect"), S("d"));
    StringArray ids_filters          = string_array_create(scratch.arena, 3, S("filters"),          S("filter"), S("f"));
    StringArray ids_output_folder    = string_array_create(scratch.arena, 3, S("output_folder"),    S("output"), S("o"));
    StringArray ids_confidence       = string_array_create(scratch.arena, 3, S("confidence"),       S("conf"),   S("c"));
    StringArray ids_thread_count     = string_array_create(scratch.arena, 3, S("thread_count"),     S("tc"),     S("j"));
    StringArray ids_buffer_size      = string_array_create(scratch.arena, 3, S("buffer_size"),      S("bs"),     S("b"));
    StringArray ids_nms_threshold    = string_array_create(scratch.arena, 3, S("nms_threshold"),    S("nms"),    S("n"));
    StringArray ids_box_padding      = string_array_create(scratch.arena, 3, S("box_padding"),      S("bp"),     S("p"));
    StringArray ids_smoothing_window = string_array_create(scratch.arena, 3, S("smoothing_window"), S("sw"),     S("s"));
    StringArray ids_texture_path     = string_array_create(scratch.arena, 3, S("texture_path"),     S("tp"),     S("t"));
    StringArray ids_block_scale      = string_array_create(scratch.arena, 3, S("block_scale"),      S("bsca"),   S("u"));
    StringArray ids_blur_strength    = string_array_create(scratch.arena, 3, S("blur_strength"),    S("bstr"),   S("w"));
    StringArray ids_bbx_output       = string_array_create(scratch.arena, 3, S("bbx_output"),       S("bbx"),    S("x"));

    String str_assets           = cmdline_get_unflagged(&cmdline, 0);
    String str_detect_types     = cmdline_get_value_first_match(&cmdline, ids_detect_types);
    String str_filters          = cmdline_get_value_first_match(&cmdline, ids_filters);
    String str_output_folder    = cmdline_get_value_first_match(&cmdline, ids_output_folder);
    String str_confidence       = cmdline_get_value_first_match(&cmdline, ids_confidence);
    String str_thread_count     = cmdline_get_value_first_match(&cmdline, ids_thread_count);
    String str_buffer_size      = cmdline_get_value_first_match(&cmdline, ids_buffer_size);
    String str_nms_threshold    = cmdline_get_value_first_match(&cmdline, ids_nms_threshold);
    String str_box_padding      = cmdline_get_value_first_match(&cmdline, ids_box_padding);
    String str_smoothing_window = cmdline_get_value_first_match(&cmdline, ids_smoothing_window);
    String str_texture_path     = cmdline_get_value_first_match(&cmdline, ids_texture_path);
    String str_block_scale      = cmdline_get_value_first_match(&cmdline, ids_block_scale);
    String str_blur_strength    = cmdline_get_value_first_match(&cmdline, ids_blur_strength);
    String str_bbx_output       = cmdline_get_value_first_match(&cmdline, ids_bbx_output);
    String str_distort_audio    = cmdline_get_value_first_match(&cmdline, ids_distort_audio);

    StringArray strs_assets       = string_split(scratch.arena, str_assets,       S(","));
    StringArray strs_detect_types = string_split(scratch.arena, str_detect_types, S(","));
    StringArray strs_filters      = string_split(scratch.arena, str_filters,      S(","));

    if(str_output_folder.len > 0)    settings.output_folder        = str_output_folder;
    if(str_confidence.len > 0)       settings.confidence_threshold = string_to_f64(str_confidence);
    if(str_nms_threshold.len > 0)    settings.nms_threshold        = string_to_f64(str_nms_threshold);
    if(str_box_padding.len > 0)      settings.box_padding          = string_to_f64(str_box_padding);
    if(str_smoothing_window.len > 0) settings.smoothing_window     = string_to_f64(str_smoothing_window);
    if(str_block_scale.len > 0)      settings.block_scale          = string_to_f64(str_block_scale);
    if(str_blur_strength.len > 0)    settings.blur_strength        = string_to_f64(str_blur_strength);
    if(str_thread_count.len > 0)     settings.thread_count         = string_to_s64(str_thread_count);
    if(str_buffer_size.len > 0)      settings.buffer_size          = string_to_s64(str_buffer_size);
    if(str_distort_audio.len > 0)
    {
        settings.distort_audio = true;
        settings.distort_audio_carrier_hz = string_to_f64(str_distort_audio);    
    }

    b32 f_help          = cmdline_has_any_flags(&cmdline, ids_help);
    b32 f_no_encode     = cmdline_has_any_flags(&cmdline, ids_no_encode);
    b32 f_debug         = cmdline_has_any_flags(&cmdline, ids_debug);
    b32 f_verbose       = cmdline_has_any_flags(&cmdline, ids_verbose);
    b32 f_quiet         = cmdline_has_any_flags(&cmdline, ids_quiet);

    if(f_help)          settings.help          = true;
    if(f_no_encode)     settings.no_encode     = true;
    if(f_debug)         settings.debug         = true;
    if(f_verbose)       settings.verbose       = true;
    if(f_quiet)         settings.quiet         = true;
    
    // create output directory if needed
    char *output_folder_cstr = string_to_cstr(scratch.arena, settings.output_folder);
    b32 output_folder_exists = os_file_exists(output_folder_cstr);

    if(!output_folder_exists)
    {
        os_file_create_directory(output_folder_cstr);
    }

    // handle input assets
    
    StringArray exts_image = string_array_create(scratch.arena, 3, S("png"), S("jpg"), S("jpeg"));
    StringArray exts_video = string_array_create(scratch.arena, 2, S("mp4"), S("mov"));
    
    for(int i = 0; i < strs_assets.count; ++i)
    {
        String str_asset = strs_assets.items[i];

        b32 is_directory = os_path_is_directory(str_asset);

        String input_folder = {0};
        StringArray file_names_arr = {0};

        if(is_directory)
        {
            input_folder = str_asset;
            file_names_arr = os_get_files_in_directory(scratch.arena, str_asset);
        }
        else
        {
            input_folder = os_path_get_directory(str_asset);
            file_names_arr = string_array_create(scratch.arena, 1, os_path_get_file(str_asset));
        }

        for(int j = 0; j < file_names_arr.count; ++j)
        {
            String file_str = file_names_arr.items[j];

            Asset *asset = &settings.assets[settings.asset_count++];
            String ext = os_path_get_extension(file_str);
            ext = string_to_lower(scratch.arena, ext);

            if(string_in_array(ext, exts_image))
            {
                asset->type = TYPE_IMAGE;
                asset->path = string_copy(arena, string_concat(arena, 3, input_folder, S("/"), file_str));
                asset->output_path = string_concat(arena, 3, settings.output_folder, S("/"), file_str);
            }
            else if(string_in_array(ext, exts_video))
            {
                asset->type = TYPE_VIDEO;
                asset->path = string_copy(arena, string_concat(arena, 3, input_folder, S("/"), file_str));
                asset->output_path = string_concat(arena, 3, settings.output_folder, S("/"), file_str);
            }
            else
            {
                asset->type = TYPE_UNSUPPORTED;
                asset->path = string_nil();
                asset->output_path = string_nil(); // TODO
            }
        }
    }

    // Detect Types

    if(strs_detect_types.count > 0)
    {
        settings.detect_type_count = 0;
        for(u32 i = 0; i < strs_detect_types.count; ++i)
        {
            String detect_type_str = string_trim(strs_detect_types.items[i]);
            DetectType type = detect_type_from_string(detect_type_str);
            if(type == DETECT_TYPE_NONE) continue;

            settings.detect_types[settings.detect_type_count++] = type;
        }
    }

    // Filters

    if(strs_filters.count > 0)
    {
        settings.filter_count = 0;
        for(u32 i = 0; i < strs_filters.count; ++i)
        {
            String filter_str = string_trim(strs_filters.items[i]);
            FilterType type = filter_from_string(filter_str);
            if(type == FILTER_TYPE_NONE) continue;

            Filter *filter = &settings.filters[settings.filter_count];
            filter->type = type;

            if(type == FILTER_TYPE_BLUR_BOX || type == FILTER_TYPE_BLUR_GAUSSIAN)
            {
                filter->blur_strength = settings.blur_strength;
            }
            else if(type == FILTER_TYPE_PIXELATE)
            {
                filter->block_scale = settings.block_scale;
            }

            settings.filter_count++;
        }
    }

    if(settings.verbose)
    {
        os_set_log_level(LOG_LEVEL_VERBOSE);
        video_set_log_level(LOG_LEVEL_VERBOSE);
    }

    if(settings.quiet)
    {
        os_set_log_level(LOG_LEVEL_QUIET);
        video_set_log_level(LOG_LEVEL_QUIET);
    }

    scratch_end(scratch);

    return settings;
}

void settings_print(Settings *settings)
{
    Temp scratch = scratch_begin();

    logi("============== Settings ===============");
    logi("%-22s %u", "Asset count", settings->asset_count);

    StringList sl = string_list_create(scratch.arena);
    string_list_add(&sl, S("["));
    for(int i = 0 ; i < settings->detect_type_count; ++i)
    {
        DetectType detect_type = settings->detect_types[i];
        string_list_add(&sl, detect_type_to_string(detect_type));
        if(i < settings->detect_type_count - 1)
            string_list_add(&sl, S(", "));
    }
    string_list_add(&sl, S("]"));
    String detect_types_str = string_list_collapse(&sl);
    logi("%-22s " STR_FMT, "Detect types", STR_ARG(detect_types_str));

    string_list_clear(&sl);

    string_list_add(&sl, S("["));
    for(int i = 0 ; i < settings->filter_count; ++i)
    {
        Filter filter = settings->filters[i];
        string_list_add(&sl, filter_to_string(filter.type));

        switch(filter.type)
        {
            case FILTER_TYPE_BLUR_BOX:
            case FILTER_TYPE_BLUR_GAUSSIAN:
                string_list_addf(&sl, ":%0.2f", filter.blur_strength);
                break;
            case FILTER_TYPE_PIXELATE:
                string_list_addf(&sl, ":%0.2f", filter.block_scale);
                break;
            case FILTER_TYPE_TEXTURE:
                string_list_addf(&sl, ":" STR_FMT, STR_ARG(filter.texture_path));
                break;
            case FILTER_TYPE_BLACKOUT:
            case FILTER_TYPE_NONE:
            default:
                break;
        }

        if(i < settings->filter_count - 1)
            string_list_add(&sl, S(", "));
    }

    string_list_add(&sl, S("]"));
    String filters_str = string_list_collapse(&sl);
    logi("%-22s " STR_FMT, "Filters", STR_ARG(filters_str));

    logi("%-22s %u",    "Thread Count",         settings->thread_count);
    logi("%-22s %f",    "Confidence Threshold", settings->confidence_threshold);
    logi("%-22s %lu B", "Buffer Size",          settings->buffer_size);
    logi("%-22s %f",    "Box Padding",          settings->box_padding);
    logi("%-22s %f",    "Smoothing Window",     settings->smoothing_window);
    logi("%-22s %s",    "No Encoding",          STR_BOOL(settings->no_encode));
    logi("%-22s %s",    "No Rotate",            STR_BOOL(settings->no_rotate));
    logi("%-22s %s",    "Debug",                settings->debug ? "ON" : "OFF");
    logi("%-22s %s",    "Verbose",              settings->verbose ? "ON" : "OFF");
    logi("%-22s " STR_FMT, "Bounding Box Output", STR_ARG(settings->bbx_output));
    logi("=======================================");

    scratch_end(scratch);
}

AssetType asset_from_string(String str)
{
    if(string_equal(str, S("image")))
        return TYPE_IMAGE;

    if(string_equal(str, S("video")))
        return TYPE_VIDEO;

    return TYPE_UNSUPPORTED;
}

String asset_to_string(AssetType type)
{
    switch(type)
    {
        case TYPE_IMAGE: return S("image");
        case TYPE_VIDEO: return S("video");
        case TYPE_UNSUPPORTED:
        default:
    }

    return S("unsupported");
}
