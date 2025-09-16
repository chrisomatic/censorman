#pragma once

#include "base.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool util_load_image(char* input_file, Image* image)
{
    int w,h,n;
    u8* data = stbi_load(input_file, &w, &h, &n, 0);

    if(!data)
    {
        LOGE("Failed to load image");
        return false;
    }

    LOGI("Loaded image! w: %d h: %d n: %d", image->w,image->h,image->n);

    if(n < 3)
    {
        LOGE("Not enough channels on image");
        return false;
    }

    // pack RGB (remove alpha channel if needed)
    image->data = (u8*)malloc(w*h*3*sizeof(u8));

    for(int i = 0; i < w*h; ++i)
    {
        image->data[i*3+0] = data[i*n+0];
        image->data[i*3+1] = data[i*n+1];
        image->data[i*3+2] = data[i*n+2];
    }
    
    image->w = w;
    image->h = h;
    image->n = 3;
    image->step = image->w*image->n;
    image->scale_x = 1.0;
    image->scale_y = 1.0;

    stbi_image_free(data);

    return true;
}

bool util_write_output(Image* image, const char* output_file)
{
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

void util_write_bbx_to_file(FILE* file, Rect* r)
{
    FileWriteU16(file, r->x);
    FileWriteU16(file, r->y);
    FileWriteU16(file, r->w);
    FileWriteU16(file, r->h);
    FileWriteU16(file, r->confidence);
    for(int i = 0; i < 5; ++i)
    {
        FileWriteU16(file, r->landmarks[i].x);
        FileWriteU16(file, r->landmarks[i].y);
    }
    FileWriteU8(file, r->interpolated ? 0x01 : 0x00);
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
