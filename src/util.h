#pragma once

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

b32 util_load_image(char* input_file, Image* image)
{
    s32 w,h,n;
    u8* data = stbi_load(input_file, &w, &h, &n, 0);

    if(!data)
    {
        loge("Failed to load image: %s", input_file);
        return false;
    }

    logv("Loaded image %s [w: %d h: %d n: %d]", input_file, image->w,image->h,image->n);

    if(n < 3)
    {
        loge("Not enough channels on image");
        return false;
    }

    // pack RGB (remove alpha channel if needed)
    image->data = (u8*)malloc(w*h*3*sizeof(u8));

    for(s32 i = 0; i < w*h; ++i)
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

b32 util_write_output(Image* image, String output_file)
{
    ArenaTemp scratch = scratch_begin();

    char * output_file_cstr = string_to_cstr(scratch.arena, output_file);

    s32 step = image->w*image->n;
    s32 res = stbi_write_png(output_file_cstr, image->w, image->h, image->n, image->data, step);

    scratch_end(scratch);

    if(res == 0)
    {
        loge("Failed to write output");
        return false;
    }
    return true;
}

void util_sort_boxes(s32 num_boxes, Box* boxes, b32 asc)
{
    // insertion sort
    s32 i, j;
    Box key;

    for (i = 1; i < num_boxes; ++i)
    {
        memcpy(&key, &boxes[i], sizeof(Box));
        j = i - 1;

        if(asc)
        {
            while (j >= 0 && boxes[j].confidence > key.confidence)
            {
                memcpy(&boxes[j+1], &boxes[j], sizeof(Box));
                j = j - 1;
            }
        }
        else
        {
            while (j >= 0 && boxes[j].confidence < key.confidence)
            {
                memcpy(&boxes[j+1], &boxes[j], sizeof(Box));
                j = j - 1;
            }
        }
        memcpy(&boxes[j+1], &key, sizeof(Box));
    }
}

void util_read_and_print_bbx_file(const char* filepath)
{
    FILE *file = fopen(filepath, "rb");
    if(!file)
        return;

        
    fclose(file);
}

void util_write_bbx_to_file(OS_File file, Box* r)
{
    os_file_write_u16(file, r->x);
    os_file_write_u16(file, r->y);
    os_file_write_u16(file, r->w);
    os_file_write_u16(file, r->h);
    os_file_write_u16(file, r->confidence);

    for(s32 i = 0; i < 5; ++i)
    {
        os_file_write_u16(file, r->landmarks[i].x);
        os_file_write_u16(file, r->landmarks[i].y);
    }
    os_file_write_u8(file, r->interpolated ? 0x01 : 0x00);
}

s32 util_get_core_count()
{
#if _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#else
    s64 nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if(nprocs < 1) nprocs = 8;
    return nprocs;
#endif
}
