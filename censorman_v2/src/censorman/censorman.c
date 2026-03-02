// 
// censorman.c
// 
// [input] ---> [detect objects] ---> [apply filters] ---> [output]
//

#include "base/base.h"
#include "os/os.h"
#include "censorman/settings.h"
#include "censorman/image.h"
#include "censorman/video.h"
#include "censorman/detect.h"
#include "censorman/filter.h"

#include "base/base.c"
#include "os/os.c"
#include "censorman/settings.c"
#include "censorman/image.c"
#include "censorman/video.c"
#include "censorman/detect.c"
#include "censorman/filter.c"

Settings settings = settings_default();

enum CM_ReturnCode
{
    CM_SUCCESS = 0,
    CM_FAILED  = 1,
};

int main(int argc, char *args[])
{
    // [input]

    settings_parse_cmd_line(&settings, args);

    timer_init();
    arena_init();
    thread_init();
    settings_print();

    // [detect objects]

    for(u32 i = 0; i < settings.asset_count; ++i)
    {
        Asset *asset = &settings.assets[i];

        if(asset->type == TYPE_IMAGE)
        {
            Image img = image_load(asset->path);

            image_scale(&img);
            image_rotate(&img);

            BoxList box_list = {0};

            for(u32 j = 0; j < settings.detect_type_count; ++j)
            {
                DetectType detect_type = settings.detect_types[j];
                detect(detect_type, &img, &box_list);
            }

            // [apply filters]
            for(u32 j = 0; j < settings.filter_count; ++j)
            {
                Filter *filter = &settings.filters[j];
                for(u64 k = 0; k < box_list.count; ++k)
                {
                    Box box = box_list.items[k];
                    filter_apply(filter->type, &img, box);
                }
            }

            // [output]
            image_write(&img, settings.output_path);

        }
        else if(asset->type == TYPE_VIDEO)
        {

        }
    }

    return CM_SUCCESS;
}

