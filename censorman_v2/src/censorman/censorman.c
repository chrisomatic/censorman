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

Arena *arena_perm;
Arena *arena_frame;

int main(int argc, char **args)
{
    // initialization
    censorman_version();
    arena_perm  = arena_create(MB(16));
    arena_frame = arena_create(MB(16));

    os_time_init();
    //os_thread_init();
    Stopwatch stopwatch = stopwatch_create();
    detect_init();

#if RUN_TESTS
    tests_run();
    return 0;
#endif

    // parse command line
    Settings settings = settings_parse(arena_perm, argc, args);
    settings_print(&settings);

    for(u32 i = 0; i < settings.asset_count; ++i)
    {
        Asset *asset = &settings.assets[i];

        logi("Processing asset [%03d/%03d]: " STR_FMT, i+1, settings.asset_count, STR_ARG(asset->path));

        if(asset->type == TYPE_IMAGE)
        {
            arena_reset(arena_frame);

            Image img_src = image_load(arena_frame, asset->path, &stopwatch);

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
                    .boxes = &box_list
                };

                detect(&detect_args);
            }

            BoxFrame box_frame = convert_list_to_box_frame(arena_frame, box_list, 1);
            logv("Box count: %u", box_frame.box_count);

            if(!settings.no_encode)
            {
                // [apply filters]
                for(u32 j = 0; j < settings.filter_count; ++j)
                {
                    Filter filter = settings.filters[j];

                    for(u64 k = 0; k < box_frame.box_count; ++k)
                    {
                        Box *box = &box_frame.boxes[k];
                        filter_apply(filter, &img_src, box);
                    }
                }

                if(settings.debug)
                {
                    for(u64 k = 0; k < box_list.count; ++k)
                    {
                        Box *box = &box_frame.boxes[k];
                        //Box *box = (Box *)list_get(&box_list, k);
                        filter_draw_debug_info(&img_src, box);
                    }
                }

                // [output]
                image_save(&img_src, asset->output_path);
            }
        }
        else if(asset->type == TYPE_VIDEO)
        {
            Video vid = video_begin(arena_frame, asset->path, asset->output_path, settings.buffer_size, settings.no_encode);

            video_print(&vid);

            for(;;)
            {
                arena_reset(arena_frame);

                // fill up buffer with frames
                video_load_frames(&vid);
                if(vid.frame_count == 0) break; // no frames left

                // determine detect frames
                ListArray frames = video_get_detect_frames(&vid, settings.smoothing_window);

                // Allocate box frames for all frames of decoded video
                BoxFrame *box_frames = PUSH_ARRAY(arena_frame, BoxFrame, vid.frame_count);

                // detect on frames
                // @THREADED
                // {
                for(u32 i = 0; i < frames.count; ++i)
                {
                    u32 frame = *(((u32 *)frames.items) + i);

                    // put frame into an image
                    Image img_src = 
                    {
                        .w = vid.w,
                        .h = vid.h,
                        .data = &vid.data[frame*vid.w*vid.h],
                        .rotation = vid.rotation,
                        .arena = vid.arena
                    };

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
                            .boxes = &box_list
                        };

                        detect(&detect_args);
                    }

                    logv("Box count: %u", box_list.count);

                    box_frames[frame] = convert_list_to_box_frame(arena_frame, box_list, frame);
                }
                // }

                // fill in gap frames
                detect_interpolate_boxes(&vid, box_frames);

                // [apply filters]
                // @THREADED
                // {
                for(u32 i = 0; i < vid.frame_count; ++i)
                {
                    Image img_src = 
                    {
                        .w = vid.w,
                        .h = vid.h,
                        .data = &vid.data[i*vid.w*vid.h],
                        .rotation = vid.rotation,
                        .arena = vid.arena
                    };

                    BoxFrame *box_frame = &box_frames[i];

                    for(u64 j = 0; j < box_frame->box_count; ++j)
                    {
                        Box *box = &box_frame->boxes[j];

                        for(u32 k = 0; k < settings.filter_count; ++k)
                        {
                            filter_apply(settings.filters[k], &img_src, box);
                        }
                    }
                }
                // }

                // [output]
                video_save_frames(&vid);
            }

            video_save_done(&vid);
            video_end(&vid);
        }
    }

    stopwatch_print(&stopwatch);

    return CM_SUCCESS;
}
