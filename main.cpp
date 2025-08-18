#include <stdio.h>
#include <pthread.h>
#include "base.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "detect.h"
#include "transform.h"

// detect.h
//    int detect_faces(Image*, Rect* rects)
// transform.h
//    void transform_black(Image*, Rect)
// net.h

// -> image -> division -> detect faces -> consolidate results -> apply transforms

void reverse_rgb_order(Image *image)
{
    printf("Reversing RGB Order... pixel count: %d\n", image->w*image->h);
    for(int i = 0; i < image->w*image->h; ++i)
    {
        int n = i*image->channels;
        u8 temp = image->data[n+0];
        image->data[n+0] = image->data[i*3+2]; // R -> B
        image->data[n+2] = temp;               // B -> R
    }
}

int main(int argc, char** args)
{
    // Initialize threads
    const int num_threads = 8;
    pthread_t* threads = (pthread_t*)calloc(num_threads,sizeof(pthread_t));

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

    int rows = 0;
    int cols = 0;

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

    int sub_width  = ceil(image.w / cols);
    int sub_height = ceil(image.h / rows);

    printf("Image sub-size: (%d, %d), config: %dx%d\n", sub_width, sub_height, rows, cols);

    int actual_thread_count = 0;
    int x = 0;
    int y = 0;

    for(int i = 0; i < num_threads; ++i)
    {
        int offset = (y*image.w*sub_height*image.channels) + x*sub_width*image.channels;
        Image sub_image = {
            .data = image.data + offset,
            .w = sub_width,
            .h = sub_height,
            .channels = image.channels
        };

#if 0
        Rect r = {x*sub_width, y*sub_height, sub_width, sub_height, 0};
        printf("Rect: (%d, %d, %d, %d)\n", r.x, r.y, r.w, r.h);
        Color c = {255, 255, 0, 255};
        transform_draw_rect(&image, r, c, false);
#endif

        printf("Starting thread %d (%d, %d) @ offset=%d\n", i, x, y, offset);

        if(pthread_create(&threads[actual_thread_count], NULL, detect_faces, (void*)&sub_image) == 0)
        {
            actual_thread_count++;
        }
        else
        {
            printf("Failed to start thread!\n");
        }

        x++;
        if(x >= cols)
        {
            x = 0;
            y++;
        }
    }

    int num_faces = 0;

    for(int i = 0; i < actual_thread_count; ++i)
    {
        void* returned_rects = NULL;
        pthread_join(threads[i], &returned_rects);

        int offset = 0;
        if(returned_rects)
        {
            int sub_faces_found = *((int*)returned_rects);
            num_faces += sub_faces_found;
            offset += sizeof(int);

            for(int j = 0; j < num_faces; ++j)
            {
                Rect* r = (Rect*)(returned_rects+offset);
                transform_black_out(&image, *r);
                offset += sizeof(Rect);
            }

        }
    }

    printf("%d faces detected\n", num_faces);

    //reverse_rgb_order(&image);


    printf("Writing output file...\n");
    int step = image.w*image.channels;
    int res = stbi_write_png("output/out.png", image.w, image.h, image.channels, image.data, step);
    if(res == 0)
    {
        fprintf(stderr, "Failed to write output!\n");
        return 1;
    }

    return 0;
}

