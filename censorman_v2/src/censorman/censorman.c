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

#include "base/base.c"
#include "os/os.c"
#include "tests/tests.c"
#include "censorman/image.c"
#include "censorman/video.c"
#include "censorman/detect.c"
#include "censorman/filter.c"
#include "censorman/settings.c"

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
Barrier   barrier = {0};
Thread    *threads = NULL;
Stopwatch stopwatch = {0};
Settings  settings = {0};
Video     vid = {0};
ListArray frames = {0};
BoxFrame  *box_frames = NULL;
b32       video_complete = false;

void *entry_point(void *params);

int main(int argc, char **args)
{
    // initialization
    os_time_init();
    os_system_init();

    arena_perm  = arena_create(MB(8));
    arena_chunk = arena_create(MB(8));
    arena_chunk_mutex = mutex_create();
    stopwatch   = stopwatch_create();

    censorman_version();

    // parse command line
    settings = settings_parse(arena_perm, argc, args);
    settings_print(&settings);

    // initialize models
    detect_init(arena_perm, settings.thread_count);

    // setup threads
    threads = PUSH_ARRAY(arena_perm, Thread, settings.thread_count);
    barrier = barrier_create(settings.thread_count);

    for(s64 thread_index = 0; thread_index < settings.thread_count; ++thread_index)
    {
        threads[thread_index] = thread_launch(entry_point, (void *)thread_index);
    }

    for(s64 thread_index = 0; thread_index < settings.thread_count; ++thread_index)
    {
        thread_join(threads[thread_index]);
    }

    return CM_SUCCESS;
}

void *entry_point(void *params)
{
    s64 thread_index = (s64)params;

    Stopwatch sw = stopwatch_create();
    Arena *arena_frame = arena_create(MB(8));

    for(u32 i = 0; i < settings.asset_count; ++i)
    {
        Asset *asset = &settings.assets[i];

        NARROW logi("Processing asset [%03d/%03d]: " STR_FMT, i+1, settings.asset_count, STR_ARG(asset->path));

        if(asset->type == TYPE_IMAGE)
        {
            NARROW
            {
                arena_reset(arena_frame);

                Image img_src = image_load(arena_frame, asset->path, &sw);

                Image img = img_src;
                img = image_scale(img, 640, 640);
                img = image_rotate(img, 0, CW);

                List box_list = list_create(arena_frame, sizeof(Box));

                // [detections]
                for(u32 j = 0; j < settings.detect_type_count; ++j)
                {
                    DetectArgs detect_args =
                    {
                        .type  = settings.detect_types[j],
                        .image = &img,
                        .thread_index = 0,
                        .boxes = &box_list
                    };

                    detect(&detect_args);
                }

                BoxFrame box_frame = convert_list_to_box_frame(arena_frame, box_list, 1);

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
                vid = video_begin(arena_chunk, asset->path, asset->output_path, settings.buffer_size, settings.no_encode);
                video_print(&vid);
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
                
                range = thread_range(thread_index, settings.thread_count, frames.count);

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

                    Image img = img_src;

                    img = image_scale(img, 640, 640);
                    img = image_rotate(img, vid.rotation, CCW);

                    List box_list = list_create(arena_frame, sizeof(Box));

                    // [detections]
                    for(u32 j = 0; j < settings.detect_type_count; ++j)
                    {
                        DetectArgs detect_args =
                        {
                            .type  = settings.detect_types[j],
                            .image = &img,
                            .thread_index = thread_index,
                            .boxes = &box_list
                        };

                        detect(&detect_args);
                    }

                    mutex_lock(&arena_chunk_mutex);
                    box_frames[frame] = convert_list_to_box_frame(arena_chunk, box_list, frame);
                    mutex_unlock(&arena_chunk_mutex);
                }

                barrier_sync(&barrier);

                NARROW
                {
                    stopwatch_begin(&sw, S("interpolate"));

                    // fill in gap frames
                    detect_interpolate_boxes(&vid, box_frames);

                    stopwatch_end(&sw, S("interpolate"));
                }

                barrier_sync(&barrier);

                stopwatch_begin(&sw, S("apply filters"));

                range = thread_range(thread_index, settings.thread_count, vid.frame_count);

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

    NARROW stopwatch_print(&sw);

    return NULL;
}
