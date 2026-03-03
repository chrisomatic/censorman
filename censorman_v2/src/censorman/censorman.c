// 
// censorman.c
// 
// [input] ---> [detect objects] ---> [apply filters] ---> [output]
//

#include "base/base.h"
#include "os/os.h"
#include "censorman/image.h"
#include "censorman/video.h"
#include "censorman/detect.h"
#include "censorman/filter.h"
#include "censorman/settings.h"

#include "base/base.c"
#include "os/os.c"
#include "censorman/image.c"
#include "censorman/video.c"
#include "censorman/detect.c"
#include "censorman/filter.c"
#include "censorman/settings.c"

#define CENSORMAN_VERSION 2

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

int main(int argc, char **args)
{
    // initialization
    censorman_version();
    os_time_init();
    //os_thread_init();
    detect_init();
    arena_perm = arena_create(MB(16));

    // parse command line
    Settings settings = settings_parse(arena_perm, argc, args);
    settings_print(&settings);

    for(u32 i = 0; i < settings.asset_count; ++i)
    {
        Asset *asset = &settings.assets[i];

        if(asset->type == TYPE_IMAGE)
        {
            Image img_src = image_load(arena_perm, asset->path);

            Image img = img_src;

            img = image_scale(img, 640, 640);
            img = image_rotate(img, 0, CW);

            List box_list = list_create(arena_perm, sizeof(Box));

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

            // [apply filters]
            for(u32 j = 0; j < settings.filter_count; ++j)
            {
                Filter filter = settings.filters[j];

                for(u64 k = 0; k < box_list.count; ++k)
                {
                    Box *box = (Box *)list_get(&box_list, k);
                    logv("Box %u: [ %u %u %u %u ]", k, box->x, box->y, box->w, box->h);
                    filter_apply(filter, &img_src, box);
                }
            }

            // [output]
            image_save(&img_src, asset->output_path);
        }
        else if(asset->type == TYPE_VIDEO)
        {

        }
    }

    return CM_SUCCESS;
}
