#pragma once

#include "base.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool util_load_image(char* input_file, Image* image)
{
    image->data = stbi_load(input_file, &image->w, &image->h, &image->n, 0);

    if(!image->data)
    {
        LOGE("Failed to load image");
        return false;
    }

    LOGI("Loaded image! w: %d h: %d n: %d", image->w,image->h,image->n);

    if(image->n < 3)
    {
        LOGE("Not enough channels on image");
        return false;
    }
    
    image->step = image->w*image->n;

    return true;
}

bool util_write_output(Image* image, const char* output_file)
{
    LOGI("Writing output file...");
    int step = image->w*image->n;
    int res = stbi_write_png(output_file, image->w, image->h, image->n, image->data, step);

    if(res == 0)
    {
        LOGE("Failed to write output");
        return false;
    }
    return true;
}

void util_sort_rects(int num_rects, Rect* rects, bool asc)
{
    // insertion sort
    int i, j;
    Rect key;

    for (i = 1; i < num_rects; ++i)
    {
        memcpy(&key, &rects[i], sizeof(Rect));
        j = i - 1;

        if(asc)
        {
            while (j >= 0 && rects[j].confidence > key.confidence)
            {
                memcpy(&rects[j+1], &rects[j], sizeof(Rect));
                j = j - 1;
            }
        }
        else
        {
            while (j >= 0 && rects[j].confidence < key.confidence)
            {
                memcpy(&rects[j+1], &rects[j], sizeof(Rect));
                j = j - 1;
            }
        }
        memcpy(&rects[j+1], &key, sizeof(Rect));
    }
}

int util_get_core_count()
{
#if _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if(nprocs < 1) nprocs = 8;
    return nprocs;
#endif
}
