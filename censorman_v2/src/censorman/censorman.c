// 
// censorman.c
// 
// [input] ---> [detect objects] ---> [apply filters] ---> [output]
//

#include "base/base.h"
#include "os/os.h"
#include "tests/tests.h"
#include "censorman/image.h"
#include "censorman/video.h"
#include "censorman/detect.h"
#include "censorman/filter.h"
#include "censorman/settings.h"
#include "censorman/bbx_file.h"

#include "base/base.c"
#include "os/os.c"
#include "tests/tests.c"
#include "censorman/image.c"
#include "censorman/video.c"
#include "censorman/detect.c"
#include "censorman/filter.c"
#include "censorman/settings.c"
#include "censorman/bbx_file.c"

#define CENSORMAN_VERSION 2
#define RUN_TESTS 0

enum CM_ReturnCode
{
    CM_SUCCESS = 0,
    CM_FAILED  = 1,
};

void censorman_version()
{
    printf("[CENSORMAN V%d]\n", CENSORMAN_VERSION);
    printf("    _O_\n");
    printf("  /|-X-|\\\n");
    printf(" /  \\_/  \\\n");
    printf("    / \\\n");
    printf("  _/   \\_\n");
}

Arena *arena_perm;  // permanent allocations
Arena *arena_chunk; // used for video frame chunks

Mutex arena_chunk_mutex = {0};

// Shared variables for threads
Barrier   barrier        = {0};
Thread    *threads       = NULL;
Stopwatch stopwatch      = {0};
Settings  settings       = {0};
OS_File   bbx_file       = {0};
Video     vid            = {0};
ListArray frames         = {0};
BoxFrame  *box_frames    = NULL;
b32       video_complete = false;

s64 entry_point(void *params);

int main(int argc, char **args)
{
    // initialization
    os_time_init();
    os_system_init();

    arena_perm  = arena_create(MB(8));
    arena_chunk = arena_create(MB(8));
    arena_chunk_mutex = mutex_create();
    stopwatch   = stopwatch_create();

    randgen_seed_with_entropy();

    // parse command line
    settings = settings_parse(arena_perm, argc, args);

    if(!settings.quiet)
    {
        censorman_version();

        if(settings.help)
        {
            settings_print_help();   
            return CM_SUCCESS;
        }
    }

    settings_print(&settings);

#if RUN_TESTS
    tests_run();
#endif

    s_thread_context.count = settings.thread_count;

    // initialize models
    detect_init(arena_perm, settings.detect_configs, settings.detect_config_count);

    // setup threads
    threads = PUSH_ARRAY(arena_perm, Thread, settings.thread_count);
    barrier = barrier_create(settings.thread_count);

    // create BBX file if specified
    if(settings.bbx_output.len > 0)
    {
        bbx_file = bbx_file_create(settings.bbx_output);
    }

    // launch threads
    for(s64 thread_index = 0; thread_index < settings.thread_count; ++thread_index)
    {
        threads[thread_index] = thread_launch(entry_point, (void *)thread_index);
    }

    // join threads
    for(s64 thread_index = 0; thread_index < settings.thread_count; ++thread_index)
    {
        thread_join(threads[thread_index]);
    }

    return CM_SUCCESS;
}

s64 entry_point(void *params)
{
    s64 thread_index = (s64)params;

    s_thread_context.index = thread_index;
    s_thread_context.count = settings.thread_count;

    Stopwatch sw = stopwatch_create();
    Arena *arena_frame = arena_create(MB(8));

    NARROW bbx_file_write_preamble(bbx_file, settings.asset_count);

    for(u32 i = 0; i < settings.asset_count; ++i)
    {
        Asset *asset = &settings.assets[i];

        NARROW logi("Processing " STR_FMT " [%03d/%03d]: " STR_FMT, STR_ARG(asset_type_to_string(asset->type)), i+1, settings.asset_count, STR_ARG(asset->path));

        if(asset->type == TYPE_IMAGE)
        {
            NARROW
            {
                arena_reset(arena_frame);

                Image img_src = image_load(arena_frame, asset->path, &sw);
                
                bbx_file_write_asset_header(bbx_file, i, asset, img_src.props.w, img_src.props.h, 0.0f, 1);
                List box_list = list_create(arena_frame, sizeof(Box));

                // [detections]
                for(u32 j = 0; j < settings.detect_config_count; ++j)
                {
                    DetectConfig *cfg = &settings.detect_configs[j];
                    Model model = detect_get_model_by_type(cfg->type);

                    Image img = img_src;
                    img = image_scale(img, model.net_w, model.net_h);
                    img = image_rotate(img, img.props.rotation, CCW);

                    detect(cfg, &img, &box_list);
                }

                BoxFrame box_frame = box_frame_from_list(arena_frame, box_list, 0);

                box_frame = box_frame_divide_into_features(arena_frame, box_frame, settings.filter_features);
                box_frame_apply_padding(box_frame, &img_src.props, settings.box_padding);

                bbx_file_write_box_frame(bbx_file, &box_frame);

                if(!settings.no_encode)
                {
                    // [apply filters]
                    for(s64 j = 0; j < settings.filter_count; ++j)
                    {
                        Filter filter = settings.filters[j];

                        for(s64 k = 0; k < box_frame.box_count; ++k)
                        {
                            Box *box = &box_frame.boxes[k];
                            filter_apply(filter, &img_src, box);
                        }
                    }

                    if(settings.debug)
                    {
                        filter_draw_debug_info(&img_src, &box_frame);
                    }

                    // [output]
                    image_save(&img_src, asset->output_path);
                }

            }
        }
        else if(asset->type == TYPE_VIDEO)
        {
            NARROW
            {
                arena_reset(arena_chunk);
                video_complete = false;

                VideoSettings vs = 
                {
                    .max_buffer_size          = settings.buffer_size,
                    .distort_audio_carrier_hz = settings.distort_audio_carrier_hz,
                    .distort_audio            = settings.distort_audio,
                    .no_encode                = settings.no_encode
                };

                vid = video_begin(arena_chunk, asset->path, asset->output_path, &vs);
                video_print(&vid);

                bbx_file_write_asset_header(bbx_file, i, asset, vid.w, vid.h, vid.fps, vid.frame_count_total);
            }

            for(;;)
            {
                NARROW
                {
                    stopwatch_begin(&sw, S("load frames"));

                    // fill up buffer with frames
                    arena_reset(arena_chunk);
                    video_load_frames(&vid);

                    if(vid.frame_count == 0)
                    {
                        video_complete = true; // no frames left
                    }
                    else
                    {
                        // determine detect frames
                        frames = video_get_detect_frames(&vid, settings.smoothing_window);

                        // Allocate box frames for all frames of decoded video
                        box_frames = PUSH_ARRAY(arena_chunk, BoxFrame, vid.frame_count);
                    }

                    stopwatch_end(&sw, S("load frames"));
                }

                barrier_sync(&barrier);

                if(video_complete) 
                    break;

                ThreadValuesRange range = {0};
                
                range = thread_range(frames.count);

                // detect on frames
                for(u32 i = range.min; i < range.max; ++i)
                {
                    arena_reset(arena_frame);

                    u32 frame = *(((u32 *)frames.items) + i);

                    // put frame into an image
                    Image img_src = 
                    {
                        .props.w = vid.w,
                        .props.h = vid.h,
                        .props.rotation = vid.rotation,
                        .data = &vid.data[frame*vid.w*vid.h],
                        .arena = arena_frame,
                        .stopwatch = &sw
                    };

                    MemoryCopy(&img_src.props_orig, &img_src.props, sizeof(ImageProps));

                    List box_list = list_create(arena_frame, sizeof(Box));

                    // [detections]
                    for(u32 j = 0; j < settings.detect_config_count; ++j)
                    {
                        DetectConfig *cfg = &settings.detect_configs[j];
                        Model model = detect_get_model_by_type(cfg->type);

                        Image img = img_src;
                        img = image_scale(img, model.net_w, model.net_h);
                        img = image_rotate(img, vid.rotation, CCW);

                        detect(cfg, &img, &box_list);
                    }

                    mutex_lock(&arena_chunk_mutex);

                    BoxFrame *box_frame = &box_frames[frame];
                    *box_frame = box_frame_from_list(arena_chunk, box_list, vid.frames_processed + frame);
                    *box_frame = box_frame_divide_into_features(arena_chunk, *box_frame, settings.filter_features);
                    box_frame_apply_padding(*box_frame, &img_src.props, settings.box_padding);
                    mutex_unlock(&arena_chunk_mutex);
                }

                barrier_sync(&barrier);

                NARROW
                {
                    stopwatch_begin(&sw, S("interpolate"));

                    // fill in gap frames
                    detect_interpolate_boxes(&vid, box_frames);

                    stopwatch_end(&sw, S("interpolate"));

                    stopwatch_begin(&sw, S("write bbx"));

                    // write bbx box frames
                    for(s64 i = 0; i < vid.frame_count; ++i)
                    {
                        bbx_file_write_box_frame(bbx_file, &box_frames[i]);
                    }

                    stopwatch_end(&sw, S("write bbx"));

                }

                barrier_sync(&barrier);

                stopwatch_begin(&sw, S("apply filters"));

                range = thread_range(vid.frame_count);

                // [apply filters]
                for(s64 i = range.min; i < range.max; ++i)
                {
                    arena_reset(arena_frame);

                    Image img_src = 
                    {
                        .props.w = vid.w,
                        .props.h = vid.h,
                        .props.rotation = vid.rotation,
                        .data = &vid.data[i*vid.w*vid.h],
                        .arena = arena_frame
                    };

                    BoxFrame *box_frame = &box_frames[i];

                    for(s64 j = 0; j < box_frame->box_count; ++j)
                    {
                        Box *box = &box_frame->boxes[j];

                        for(s64 k = 0; k < settings.filter_count; ++k)
                        {
                            filter_apply(settings.filters[k], &img_src, box);
                        }
                    }

                    if(settings.debug)
                    {
                        filter_draw_debug_info(&img_src, box_frame);
                    }
                }

                stopwatch_end(&sw, S("apply filters"));

                NARROW
                {
                    // [output]
                    stopwatch_begin(&sw, S("save video"));
                    video_save_frames(&vid);
                    stopwatch_end(&sw, S("save video"));
                }

                barrier_sync(&barrier);
            }

            NARROW
            {
                video_save_done(&vid);
                video_end(&vid);
            }
        }
    }

    NARROW 
    {
        bbx_file_close(bbx_file);
        // bbx_file_parse_and_print(settings.bbx_output); // @TEMP
        if(settings.verbose) stopwatch_print(&sw);

        logi("Complete! Processed files in folder: '" STR_FMT "'", STR_ARG(settings.output_folder));
    }

    return 0;
}
