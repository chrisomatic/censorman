
Settings settings_default(void)
{
    Settings settings = {0};

    settings.filters[0].type       = FILTER_TYPE_BLUR_BOX;
    settings.filters[0].param      = 0.45f;
    settings.filters[0].elliptical = false;

    settings.filter_count = 1;

    settings.output_folder = S("output");

    settings.thread_count         = os_system_info.logical_processor_count;
    settings.buffer_size          = MB(512);
    settings.box_padding          = 0.15f;
    settings.blur_strength        = 0.50f;
    settings.block_scale          = 0.12f;
    settings.smoothing_window     = 0.200f; // 200ms

    settings.thumbnail_width  = 250;
    settings.thumbnail_height = 250;

    settings.detect_configs[0].type = DETECT_TYPE_FACE;
    settings.detect_configs[0].threshold_confidence = 0.42f;
    settings.detect_configs[0].threshold_nms        = 0.45f;
    settings.detect_config_count = 1;

    settings.no_encode = false;
    settings.debug     = false;
    settings.no_labels = false;
    settings.verbose   = false;
    settings.stopwatch = false;
    settings.quiet     = false;
    settings.report    = false;
    settings.help      = false;

    return settings;
}

Settings settings_parse(Arena *arena, int argc, char **args)
{
    Settings settings = settings_default();

    Temp scratch = scratch_begin();

    CmdLine cmdline = cmdline_parse(scratch.arena, argc, args);

    StringArray ids_help             = string_array_create(scratch.arena, 2, S("help"),             S("h"));
    StringArray ids_no_encode        = string_array_create(scratch.arena, 2, S("no_encode"),        S("ne"));
    StringArray ids_quiet            = string_array_create(scratch.arena, 2, S("quiet"),            S("q"));
    StringArray ids_debug            = string_array_create(scratch.arena, 2, S("debug"),            S("db"));
    StringArray ids_no_labels        = string_array_create(scratch.arena, 2, S("no_labels"),        S("nl"));
    StringArray ids_verbose          = string_array_create(scratch.arena, 2, S("verbose"),          S("vb"));
    StringArray ids_stopwatch        = string_array_create(scratch.arena, 2, S("stopwatch"),        S("sw"));
    StringArray ids_distort_audio    = string_array_create(scratch.arena, 2, S("distort_audio"),    S("da"));
    StringArray ids_detect_types     = string_array_create(scratch.arena, 2, S("detect"),           S("d"));
    StringArray ids_filters          = string_array_create(scratch.arena, 2, S("filter"),           S("f"));
    StringArray ids_output_folder    = string_array_create(scratch.arena, 2, S("output_folder"),    S("o"));
    StringArray ids_thread_count     = string_array_create(scratch.arena, 2, S("thread_count"),     S("j"));
    StringArray ids_buffer_size      = string_array_create(scratch.arena, 2, S("buffer_size"),      S("bs"));
    StringArray ids_box_padding      = string_array_create(scratch.arena, 2, S("box_padding"),      S("bp"));
    StringArray ids_smoothing_window = string_array_create(scratch.arena, 2, S("smoothing_window"), S("sw"));
    StringArray ids_texture_path     = string_array_create(scratch.arena, 2, S("texture_path"),     S("tp"));
    StringArray ids_bbx_output       = string_array_create(scratch.arena, 2, S("bbx_file"),         S("bbx"));
    StringArray ids_bbx_file_format  = string_array_create(scratch.arena, 2, S("bbx_file_format"),  S("bff"));
    StringArray ids_facial_features  = string_array_create(scratch.arena, 2, S("facial_features"),  S("ff"));
    StringArray ids_thumbnail        = string_array_create(scratch.arena, 2, S("thumbnail"),        S("tn"));
    StringArray ids_elliptical       = string_array_create(scratch.arena, 2, S("elliptical"),       S("el"));
    StringArray ids_report           = string_array_create(scratch.arena, 2, S("report"),           S("r"));

    String str_assets           = cmdline_get_unflagged(&cmdline, 0);
    String str_detect_types     = cmdline_get_value_first_match(&cmdline, ids_detect_types);
    String str_filters          = cmdline_get_value_first_match(&cmdline, ids_filters);
    String str_output_folder    = cmdline_get_value_first_match(&cmdline, ids_output_folder);
    String str_thread_count     = cmdline_get_value_first_match(&cmdline, ids_thread_count);
    String str_buffer_size      = cmdline_get_value_first_match(&cmdline, ids_buffer_size);
    String str_box_padding      = cmdline_get_value_first_match(&cmdline, ids_box_padding);
    String str_smoothing_window = cmdline_get_value_first_match(&cmdline, ids_smoothing_window);
    String str_texture_path     = cmdline_get_value_first_match(&cmdline, ids_texture_path);
    String str_bbx_output       = cmdline_get_value_first_match(&cmdline, ids_bbx_output);
    String str_distort_audio    = cmdline_get_value_first_match(&cmdline, ids_distort_audio);
    String str_facial_features  = cmdline_get_value_first_match(&cmdline, ids_facial_features);
    String str_thumbnail        = cmdline_get_value_first_match(&cmdline, ids_thumbnail);

    StringArray strs_assets          = string_split(scratch.arena, str_assets,          S(","));
    StringArray strs_detect_types    = string_split(scratch.arena, str_detect_types,    S(","));
    StringArray strs_filters         = string_split(scratch.arena, str_filters,         S(","));
    StringArray strs_facial_features = string_split(scratch.arena, str_facial_features, S(","));

    if(str_output_folder.len > 0)    settings.output_folder    = str_output_folder;
    if(str_bbx_output.len > 0)       settings.bbx_output       = str_bbx_output;
    if(str_texture_path.len > 0)     settings.texture_path     = str_texture_path;
    if(str_box_padding.len > 0)      settings.box_padding      = string_to_f64(str_box_padding);
    if(str_smoothing_window.len > 0) settings.smoothing_window = string_to_f64(str_smoothing_window);
    if(str_thread_count.len > 0)     settings.thread_count     = string_to_s64(str_thread_count);
    if(str_buffer_size.len > 0)      settings.buffer_size      = string_to_s64(str_buffer_size);
    if(str_distort_audio.len > 0)
    {
        settings.distort_audio = true;
        settings.distort_audio_carrier_hz = string_to_f64(str_distort_audio);    
    }

    b32 f_help       = cmdline_has_any_flags(&cmdline, ids_help) | (cmdline.args.count == 0);
    b32 f_bbx_format = cmdline_has_any_flags(&cmdline, ids_bbx_file_format);
    b32 f_no_encode  = cmdline_has_any_flags(&cmdline, ids_no_encode);
    b32 f_debug      = cmdline_has_any_flags(&cmdline, ids_debug);
    b32 f_no_labels  = cmdline_has_any_flags(&cmdline, ids_no_labels);
    b32 f_verbose    = cmdline_has_any_flags(&cmdline, ids_verbose);
    b32 f_stopwatch  = cmdline_has_any_flags(&cmdline, ids_stopwatch);
    b32 f_thumbnail  = cmdline_has_any_flags(&cmdline, ids_thumbnail);
    b32 f_elliptical = cmdline_has_any_flags(&cmdline, ids_elliptical);
    b32 f_report     = cmdline_has_any_flags(&cmdline, ids_report);
    b32 f_quiet      = cmdline_has_any_flags(&cmdline, ids_quiet);

    if(f_help)       settings.help       = true;
    if(f_bbx_format) settings.bbx_print_format = true;
    if(f_no_encode)  settings.no_encode  = true;
    if(f_debug)      settings.debug      = true;
    if(f_no_labels)  settings.no_labels  = true;
    if(f_verbose)    settings.verbose    = true;
    if(f_stopwatch)  settings.stopwatch  = true;
    if(f_thumbnail)  settings.thumbnail  = true;
    if(f_elliptical) settings.elliptical = true;
    if(f_report)     settings.report     = true;
    if(f_quiet)      settings.quiet      = true;

    // create output directory if needed
    char *output_folder_cstr = string_to_cstr(scratch.arena, settings.output_folder);
    b32 output_folder_exists = os_file_exists(output_folder_cstr);

    if(!output_folder_exists)
    {
        os_file_create_directory(output_folder_cstr);
    }

    if(!STR_EMPTY(str_thumbnail))
    {
        String str_thumbnail_lower = string_to_lower(scratch.arena, str_thumbnail);
        StringArray dimensions = string_split(scratch.arena, str_thumbnail_lower, S("x"));

        s64 dim[2] = {0};
        if(dimensions.count == 1)
        {
            dim[0] = string_to_s64(dimensions.items[0]);
            dim[1] = dim[0];
        }
        else if(dimensions.count == 2)
        {
            dim[0] = string_to_s64(dimensions.items[0]);
            dim[1] = string_to_s64(dimensions.items[1]);
        }

        if(dim[0] > 0) settings.thumbnail_width  = dim[0];
        if(dim[1] > 0) settings.thumbnail_height = dim[1];
    }

    // handle input assets

    StringArray exts_image = string_array_create(scratch.arena, 10, S("png"), S("jpg"), S("jpeg"), S("bmp"), S("gif"), S("psd"), S("tga"), S("hdr"), S("pic"),  S("pnm"));
    StringArray exts_video = string_array_create(scratch.arena, 10, S("mp4"), S("m4v"), S("m4a"),  S("mov"), S("avi"), S("wmv"), S("wma"), S("asf"), S("webm"), S("mkv"));
    StringArray exts_pdf   = string_array_create(scratch.arena, 1,  S("pdf"));
    
    for(int i = 0; i < strs_assets.count; ++i)
    {
        String str_asset = strs_assets.items[i];

        b32 is_directory = os_path_is_directory(str_asset);

        StringArray file_names_arr = {0};

        if(is_directory)
        {
            str_asset = os_path_remove_trailing_slashes(str_asset);
            file_names_arr = os_get_files_in_directory(scratch.arena, str_asset, true);
        }
        else
        {
            file_names_arr = string_array_create(scratch.arena, 1, str_asset);
        }

        for(int j = 0; j < file_names_arr.count; ++j)
        {
            String file_str = file_names_arr.items[j];

            Asset *asset = &settings.assets[settings.asset_count];
            String ext = os_path_get_extension(file_str);
            ext = string_to_lower(scratch.arena, ext);

            if(string_in_array(ext, exts_image))
            {
                asset->type = TYPE_IMAGE;
                asset->path = file_str;
                asset->output_path = string_concat(arena, 3, settings.output_folder, OS_PATH_SLASH_STR, os_path_get_file_part(file_str));
                settings.asset_count++;
            }
            else if(string_in_array(ext, exts_video))
            {
                // forcing the output extension to be mp4
                asset->type = TYPE_VIDEO;
                asset->path = file_str;
                asset->output_path = string_concat(arena, 4, settings.output_folder, OS_PATH_SLASH_STR, os_path_get_file_part_without_ext(file_str), S(".mp4"));
                settings.asset_count++;
            }
            else if(string_in_array(ext, exts_pdf))
            {
                asset->type = TYPE_PDF;
                asset->path = file_str;
                asset->output_path = string_concat(arena, 3, settings.output_folder, OS_PATH_SLASH_STR, os_path_get_file_part(file_str));
                settings.asset_count++;
            }
        }
    }

    // Detect Types

    if(strs_detect_types.count > 0)
    {
        settings.detect_config_count = 0;
        for(u32 i = 0; i < strs_detect_types.count; ++i)
        {
            String detect_type_str = string_trim(strs_detect_types.items[i]);

            // check for :<param1>:<param2>
            StringArray detect_type_params = string_split(scratch.arena, detect_type_str, S(":"));
            detect_type_str = detect_type_params.items[0];
            f64 param1 = 0.0;
            if(detect_type_params.count > 1)
            {
                param1 = string_to_f64(detect_type_params.items[1]);
                param1 = CLAMP(param1, 0.0, 1.0);
            }

            f64 param2 = 0.0;
            if(detect_type_params.count > 2)
            {
                param2 = string_to_f64(detect_type_params.items[2]);
                param2 = CLAMP(param2, 0.0, 1.0);
            }

            DetectType type = detect_type_from_string(detect_type_str);
            if(type == DETECT_TYPE_NONE) continue;

            DetectConfig *cfg = &settings.detect_configs[settings.detect_config_count++];

            cfg->type = type;

            // set default configurations
            switch(type)
            {
                case DETECT_TYPE_FACE:
                    cfg->threshold_confidence = 0.42f;
                    cfg->threshold_nms        = 0.45f;
                    break;
                case DETECT_TYPE_FACE_10G:
                    cfg->threshold_confidence = 0.42f;
                    cfg->threshold_nms        = 0.45f;
                    break;
                case DETECT_TYPE_PERSON:
                    cfg->threshold_confidence = 0.50f;
                    cfg->threshold_nms        = 0.50f;
                    break;
                case DETECT_TYPE_LICENSE_PLATE:
                    cfg->threshold_confidence = 0.50f;
                    cfg->threshold_nms        = 0.45f;
                    break;
                case DETECT_TYPE_NUDITY:
                    cfg->threshold_confidence = 0.25f;
                    cfg->threshold_nms        = 0.45f;
                    break;
                default:
                    cfg->threshold_confidence = 0.25f;
                    cfg->threshold_nms        = 0.45f;
                    break;
            }

            if(param1 > 0.0)
            {
                cfg->threshold_confidence = param1;
            }

            if(param2 > 0.0)
            {
                cfg->threshold_nms = param2;
            }
        }
    }

    // Filters

    if(strs_filters.count > 0)
    {
        settings.filter_count = 0;
        for(u32 i = 0; i < strs_filters.count; ++i)
        {
            String filter_str = string_trim(strs_filters.items[i]);

            // check for :<parameter>:<elliptical>
            StringArray filter_params = string_split(scratch.arena, filter_str, S(":"));
            filter_str = filter_params.items[0];

            f64 param1 = 0.0;
            s64 param2 = 0;

            if(filter_params.count > 1)
            {
                param1 = string_to_f64(filter_params.items[1]);
                param1 = CLAMP(param1, 0.0, 1.0);
            }

            if(filter_params.count > 2)
            {
                param2 = string_to_s64(filter_params.items[2]);
                param2 = CLAMP(param2, 0.0, 1.0);
            }

            FilterType type = filter_from_string(filter_str);
            if(type == FILTER_TYPE_NONE) continue;

            Filter *filter = &settings.filters[settings.filter_count];
            filter->type = type;

            if(type == FILTER_TYPE_BLUR_BOX || type == FILTER_TYPE_BLUR_GAUSSIAN)
            {
                filter->param = param1 == 0.0 ? settings.blur_strength : param1;
            }
            else if(type == FILTER_TYPE_PIXELATE)
            {
                filter->param = param1 == 0.0 ? settings.block_scale : param1;
            }

            if(param2 > 0)
            {
                filter->elliptical = true;
            }
            else {
                filter->elliptical = settings.elliptical;
            }

            settings.filter_count++;
        }
    }
    else
    {
        settings.filters[0].elliptical = settings.elliptical;
    }
    
    // Filter Features

    if(strs_facial_features.count > 0)
    {
        settings.facial_features = 0x0;

        for(s64 i = 0; i < strs_facial_features.count; ++i)
        {
            String facial_feature = string_trim(strs_facial_features.items[i]);
            FacialFeature ff = facial_feature_from_string(facial_feature);

            settings.facial_features |= ff;
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
    for(int i = 0 ; i < settings->detect_config_count; ++i)
    {
        DetectConfig *cfg = &settings->detect_configs[i];
        string_list_add(&sl, detect_type_to_string(cfg->type));
        string_list_addf(&sl, "(%0.2f,%0.2f)", cfg->threshold_confidence, cfg->threshold_nms);
        if(i < settings->detect_config_count - 1)
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
                string_list_addf(&sl, ":%0.2f:%d", filter.param, filter.elliptical);
                break;
            case FILTER_TYPE_PIXELATE:
                string_list_addf(&sl, ":%0.2f:%d", filter.param, filter.elliptical);
                break;
            case FILTER_TYPE_TEXTURE:
                string_list_addf(&sl, ":" STR_FMT ":%d", STR_ARG(filter.texture_path), filter.elliptical);
                break;
            case FILTER_TYPE_BLACKOUT:
                string_list_addf(&sl, ":%d", filter.elliptical);
            case FILTER_TYPE_NONE:
            default:
                break;
        }

        if(i < settings->filter_count - 1)
            string_list_add(&sl, S(", "));
    }

    string_list_add(&sl, S("]"));
    String filters_str = string_list_collapse(&sl);

    logi("%-22s " STR_FMT,      "Filters",              STR_ARG(filters_str));
    logi("%-22s 0x%02X",        "Facial Features",      settings->facial_features);
    logi("%-22s %u",            "Thread Count",         settings->thread_count);
    logi("%-22s %lu B",         "Buffer Size",          settings->buffer_size);
    logi("%-22s %f",            "Box Padding",          settings->box_padding);
    logi("%-22s %f",            "Smoothing Window",     settings->smoothing_window);
    logi("%-22s %s (%0.2f Hz)", "Distort Audio",        STR_BOOL(settings->distort_audio), settings->distort_audio_carrier_hz);
    logi("%-22s %s",            "No Encoding",          STR_BOOL(settings->no_encode));
    logi("%-22s %s",            "Debug",                STR_BOOL(settings->debug));
    logi("%-22s %s",            "Verbose",              STR_BOOL(settings->verbose));
    logi("%-22s %s (%dx%d)",    "Thumbnail",            STR_BOOL(settings->thumbnail), settings->thumbnail_width, settings->thumbnail_height);
    logi("%-22s %s",            "Elliptical",           STR_BOOL(settings->elliptical));
    logi("%-22s " STR_FMT,      "Output Folder",        STR_ARG(settings->output_folder));
    logi("%-22s " STR_FMT,      "Texture Path",         STR_ARG(settings->texture_path));
    logi("%-22s " STR_FMT,      "Bounding Box Output",  STR_ARG(settings->bbx_output));
    logi("=======================================");

    scratch_end(scratch);
}

AssetType asset_type_from_string(String str)
{
    if(string_equal(str, S("image")))
        return TYPE_IMAGE;

    if(string_equal(str, S("video")))
        return TYPE_VIDEO;

    if(string_equal(str, S("pdf")))
        return TYPE_PDF;

    return TYPE_UNSUPPORTED;
}

String asset_type_to_string(AssetType type)
{
    switch(type)
    {
        case TYPE_IMAGE: return S("image");
        case TYPE_VIDEO: return S("video");
        case TYPE_PDF:   return S("pdf");
        case TYPE_UNSUPPORTED:
        default: break;
    }

    return S("unsupported");
}

void settings_print_help(void)
{
    os_printf("\nUSAGE\n");
    os_printf("    censorman <asset_path> [options | flags]\n");
    os_printf("\nASSET_PATH\n");
    os_printf("    Accepts comma-separated list of image and video file paths, or a folder path (searches recursively).\n");
    os_printf("    * Supported image formats: [ PNG, JPG, BMP, PSD, GIF, TGA, HDR, PIC, PNM ]\n");
    os_printf("    * Supported video formats: [ MP4, MOV ]\n");
    os_printf("\nOPTIONS\n");
    os_printf("    --detect [-d] <detect-types>\n");
    os_printf("        a comma-separated list of detect types [face,face_10g,person,license_plate,nudity]\n");
    os_printf("        default: face\n");
    os_printf("        Each detect type can specify up to two optional parameters with ':' between them\n");
    os_printf("        The first parameter is confidence threshold\n");
    os_printf("        The second parameter is NMS IOU threshold\n");
    os_printf("\n");
    os_printf("    --filter [-f] <filters>\n");
    os_printf("        a comma-separated list of filters [blur,gaussian_blur,pixelate,scramble,blackout,texture]\n");
    os_printf("        default: blur\n");
    os_printf("        Each filter can specify an optional parameter with ':' followed by an optional flag to enable an elliptical mask (e.g blur:0.20:1)\n");
    os_printf("        This parameter indicates 'blur_strength' for blur and gaussian_blur, or\n");
    os_printf("        'block_scale' with pixelate\n");
    os_printf("\n");
    os_printf("    --output_folder [-o] <output_folder_path>\n");
    os_printf("        Specify the output folder of processed assets (images/videos).\n");
    os_printf("        If the folder doesn't exist, it will be created\n");
    os_printf("        Default: output\n");
    os_printf("\n");
    os_printf("    --distort_audio [-da] <distortion_hz>\n");
    os_printf("        Apply a ring modulation to the audio stream of a video file.\n");
    os_printf("        A typical range for distortion is 150 Hz to 300 Hz\n");
    os_printf("\n");
    os_printf("    --thread_count [-j] <thread_count>\n");
    os_printf("        Specify thread count. Used for video processing. Images are processed single-threaded.\n");
    os_printf("        Default: Number of logical cores\n");
    os_printf("\n");
    os_printf("    --buffer_size [-bs] <buffer_size_bytes>\n");
    os_printf("        Set the maximum buffer size for video frames. Video frames are loaded in chunks, so\n");
    os_printf("        larger chunks allow more frames of a video to be decoded at a time.\n");
    os_printf("        Default: 536870912 (512 MB)\n");
    os_printf("\n");
    os_printf("    --box_padding [-bp] <box_padding_percent>\n");
    os_printf("        Specify the box padding percentage. Padding is added at the end of the detection.\n");
    os_printf("        Default: 0.15 (15 %%)\n");
    os_printf("\n");
    os_printf("    --smoothing_window [-sw] <smoothing_window_seconds>\n");
    os_printf("        Used for video interpolation of detection boxes. Interpolation is an exponentional smooth.\n");
    os_printf("        There is also an implicit first stage to find discontinuities and fast motion\n");
    os_printf("        and schedule those frames for detection\n");
    os_printf("        Default: 0.200 (200 ms)\n");
    os_printf("\n");
    os_printf("    --texture_path [-tp] <texture_path>\n");
    os_printf("        Supply a path to an image file that is stretched over bounding boxes on the output.\n");
    os_printf("        Used with the --filter texture option\n");
    os_printf("\n");
    os_printf("    --bbx_file [-bbx] <bbx_file_path>\n");
    os_printf("        Bounding-box output file. If specified, the detect box data will be written to a file\n");
    os_printf("\n");
    os_printf("    --facial_features [-ff] <facial_features>\n");
    os_printf("        Break the facial box into sub-boxes. Comma-delimited list of ['eyes','nose','mouth','cheeks','forehead']\n");
    os_printf("\n");
    os_printf("    --thumbnail [-tn] [<thumbnail_width>x<thumbnail_height>]\n");
    os_printf("        Produce a *_thumbnail.png for each asset. Default thumbnail dimensions are 250x250\n");
    os_printf("        Maintains aspect ratio, so the longest dimension will be set and the other\n");
    os_printf("        adjusted to that\n");
    os_printf("\nFLAGS\n");
    os_printf("    --bbx_file_format [-bff]  Print information about the file format for BBX files\n");
    os_printf("    --no_encode [-ne]         Disable the writing of the processed output file(s)\n");
    os_printf("    --debug [-db]             Enable debug info markout on output. Draws boxes on output with labels\n");
    os_printf("    --no_labels [-nl]         Use with --debug flag to exclude the labels on the debug markup\n");
    os_printf("    --verbose [-vb]           Turn on verbose console prints\n");
    os_printf("    --elliptical [-el]        Mask all filters to a rounded elliptical shape\n");
    os_printf("    --stopwatch [-sw]         Turn on stopwatch prints for timing information\n");
    os_printf("    --quiet [-q]              Disable all console prints\n");
    os_printf("    --help [-h]               Display this help output\n");
    os_printf("\nEXAMPLES\n");
    os_printf("    1.  Just run default settings on test1.png (detect faces and apply box blur)\n");
    os_printf("          censorman assets/images/test1.png\n");
    os_printf("\n");
    os_printf("    2.  Detect persons on all supported files in assets/images and pixelate boxes with a block scale of 0.12\n");
    os_printf("          censorman assets/images -d person -f pixelate:0.12\n");
    os_printf("\n");
    os_printf("    3.  Apply box blur to faces and license plates in vid1.mp4 with debug markup\n");
    os_printf("          censorman assets/videos/vid1.mp4 -d face,license_plate -f blur --debug\n");
    os_printf("\n");
}
