#include <stdio.h>
#include "base.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "detect.h"
#include "util.h"

// detect.h
//    int detect_faces(Image*, Rect* rects)
// transform.h
//    void transform_black(Image*, Rect)
// net.h

// -> image -> division -> detect faces -> consolidate results -> apply transforms

void reverse_rgb_order(Image *image)
{
    printf("Reversing RGB Order... %d\n", image->w*image->h);
    for(U32 i = 0; i < image->w*image->h; ++i)
    {
        U32 n = i*image->channels;
        U8 temp = image->data[n+0];
        image->data[n+0] = image->data[i*3+2]; // R -> B
        image->data[n+2] = temp;               // B -> R
    }
}

int main(int argc, char** args)
{
    // Initialize threads
    const U32 num_threads = 8;
    pthread_t* threads = calloc(num_threads,sizeof(pthread_t));

    Image image = {0};

    image.data = stbi_load("images/test1.jpg", &image.w, &image.h, &image.channels, 0);

    if(!image.data)
    {
        fprintf(stderr, "Failed to load image\n");
        return 1;
    }

    printf("Loaded image! w: %d h: %d n: %d\n", image.w,image.h,image.channels);

    if(image.channels < 3)
    {
        fprintf(stderr, "Not enough channels on image.\n");
        return 1;
    }

    reverse_rgb_order(&image);

    printf("Detecting faces...\n");


    // Determine image subdivision

    bool is_horiz = (image.w >= image.h);

    U32 rows = 0;
    U32 cols = 0;

    switch(num_threads)
    {
        case 1: // 1x1
            rows = 1;
            cols = 1;
            break;
        case 2: // 1x2 or 2x1
            rows = is_horiz ? 1 : 2;
            cols = is_horiz ? 2 : 1;
            break;
        case 3: // 1x3 or 3x1
            rows = is_horiz ? 1 : 3;
            cols = is_horiz ? 3 : 1;
            break;
        case 4: // 2x2
            rows = 2;
            cols = 2;
            break;
        case 5: // 1x5 or 5x1
            rows = is_horiz ? 1 : 5;
            cols = is_horiz ? 5 : 1;
            break;
        case 6: // 2x3 or 3x2
            rows = is_horiz ? 2 : 3;
            cols = is_horiz ? 3 : 2;
            break;
        case 7: // 1x7 or 7x1
            rows = is_horiz ? 1 : 7;
            cols = is_horiz ? 7 : 1;
            break;
        case 8: // 2x4 or 4x2
            rows = is_horiz ? 2 : 4;
            cols = is_horiz ? 4 : 2;
            break;
        case 9: // 3x3
            rows = 3;
            cols = 3;
            break;
        case 10: // 2x5 or 5x2
            rows = is_horiz ? 2 : 5;
            cols = is_horiz ? 5 : 2;
            break;
        case 11: // 1x11 or 11x1
            rows = is_horiz ? 1 : 11;
            cols = is_horiz ? 11 : 1;
            break;
        case 12: // 3x4 or 4x3
            rows = is_horiz ? 3 : 4;
            cols = is_horiz ? 4 : 3;
            break;
        case 13: // 1x13 or 13x1
            rows = is_horiz ? 1 : 13;
            cols = is_horiz ? 13 : 1;
            break;
        case 14: // 2x7 or 7x2
            rows = is_horiz ? 2 : 7;
            cols = is_horiz ? 7 : 2;
            break;
        case 15: // 3x5 or 5x3
            rows = is_horiz ? 3 : 5;
            cols = is_horiz ? 5 : 3;
            break;
        case 16: // 4x4
            rows = 4;
            cols = 4;
            break;
        default:
            rows = is_horiz ? 1 : num_threads;
            cols = is_horiz ? num_threads : 1;
    }

    U32 actual_thread_count = 0;
    U32 x = 0;
    U32 y = 0;

    sub_width  = ceil(image.w / cols);
    sub_height = ceil(image.h / rows);

    for(U32 i = 0; i < num_threads; ++i)
    {
        DetectInfo di = {
            .image = &image,
            .x_offset = x,
            .y_offset = y, 
            .step = sub_width
        };

        if(pthread_create(&threads[actual_thread_count], NULL, detect_faces, &di) == 0)
        {
            actual_thread_count++;
        }
        else
        {
            printf("Failed to start thread!\n");
        }

        y++;
        if(y >= cols) {
            y = 0;
            x++;
        }
    }

    U32 num_faces = 0; // @TEMP
    
    void* returned_rects = NULL;

    for(U32 i = 0; i < actual_thread_count; ++i)
    {
        pthread_join(threads[i], &returned_rects);
    }

    printf("%d faces detected\n", num_faces);
    U32 step = image.w*image.channels;

    for(U32 i = 0; i < num_faces; ++i)
    {
        Rect* r = &rects[i];
        if(r->confidence < 80) continue;

        // black out pixels (for now)
        for(U32 j = r->y; j < r->y + r->h; ++j)
        {
            for(U32 k = r->x; k < r->x + r->w; ++k)
            {
                U32 kn = k*image.channels;
                image.data[j*step+kn+0] = 0x00;
                image.data[j*step+kn+1] = 0x00;
                image.data[j*step+kn+2] = 0x00;
            }
        }
    }

    reverse_rgb_order(&image);

    printf("Writing output file...\n");
    U32 res = stbi_write_png("output/out.png", image.w, image.h, image.channels, image.data, step);
    if(res == 0)
    {
        fprintf(stderr, "Failed to write output!\n");
        return 1;
    }

    return 0;
}

