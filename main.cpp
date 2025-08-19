#include <stdio.h>
#include <pthread.h>
#include "base.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "detect.h"
#include "transform.h"

#define MAX_ARENAS 64
Arena* arenas[MAX_ARENAS] = {0};

// -> image -> division -> detect faces -> consolidate results -> apply transforms

void reverse_rgb_order(Image *image)
{
    LOGI("Reversing RGB Order... pixel count: %d", image->w*image->h);
    for(int i = 0; i < image->w*image->h; ++i)
    {
        int n = i*image->n;
        u8 temp = image->data[n+0];
        image->data[n+0] = image->data[i*3+2]; // R -> B
        image->data[n+2] = temp;               // B -> R
    }
}

int main(int argc, char** args)
{
    // init

    timer_init();
    log_init(0);

    const int num_threads = 7;
    pthread_t* threads = (pthread_t*)calloc(num_threads,sizeof(pthread_t));

    Image image = {0};

    image.data = stbi_load("images/test1.jpg", &image.w, &image.h, &image.n, 0);

    if(!image.data)
    {
        LOGE("Failed to load image");
        return 1;
    }

    LOGI("Loaded image! w: %d h: %d n: %d", image.w,image.h,image.n);

    if(image.n < 3)
    {
        LOGE("Not enough channels on image");
        return 1;
    }

    reverse_rgb_order(&image);

    // Determine image subdivision

    bool is_horiz = (image.w >= image.h);

    int rows = 0;
    int cols = 0;

    switch(num_threads)
    {
        case 1:  rows = 1; cols = 1; break;
        case 2:  rows = is_horiz ? 1 : 2; cols = is_horiz ? 2 : 1; break;
        case 3:  rows = is_horiz ? 1 : 3; cols = is_horiz ? 3 : 1; break;
        case 4:  rows = 2; cols = 2; break;
        case 5:  rows = is_horiz ? 1 : 5; cols = is_horiz ? 5 : 1; break;
        case 6:  rows = is_horiz ? 2 : 3; cols = is_horiz ? 3 : 2; break;
        case 7:  rows = is_horiz ? 1 : 7; cols = is_horiz ? 7 : 1; break;
        case 8:  rows = is_horiz ? 2 : 4; cols = is_horiz ? 4 : 2; break;
        case 9:  rows = 3; cols = 3; break;
        case 10: rows = is_horiz ? 2 : 5;  cols = is_horiz ? 5  : 2; break;
        case 11: rows = is_horiz ? 1 : 11; cols = is_horiz ? 11 : 1; break;
        case 12: rows = is_horiz ? 3 : 4;  cols = is_horiz ? 4  : 3; break;
        case 13: rows = is_horiz ? 1 : 13; cols = is_horiz ? 13 : 1; break;
        case 14: rows = is_horiz ? 2 : 7;  cols = is_horiz ? 7  : 2; break;
        case 15: rows = is_horiz ? 3 : 5;  cols = is_horiz ? 5  : 3; break;
        case 16: rows = 4; cols = 4; break;
        default:
            rows = is_horiz ? 1 : num_threads;
            cols = is_horiz ? num_threads : 1;
    }

    int sub_width  = ceil(image.w / cols);
    int sub_height = ceil(image.h / rows);

    LOGI("Image sub-size: (%d, %d), config: %dx%d", sub_width, sub_height, rows, cols);

    Image* sub_images[num_threads] = {0};
    u8 detect_buffers[num_threads][0x9000] = {0};

    for(int i = 0; i < num_threads; ++i)
    {
        arenas[i] = arena_create(ARENA_SIZE_MEDIUM);
        sub_images[i] = (Image*)arena_alloc(arenas[i], sizeof(Image));
    }

    int actual_thread_count = 0;
    int x = 0;
    int y = 0;

    LOGI("Detecting faces...");

    for(int i = 0; i < num_threads; ++i)
    {
        Arena* arena = arenas[actual_thread_count];
        pthread_t* thread = &threads[actual_thread_count];
        Image* sub_image = sub_images[actual_thread_count];

        int offset = (y*image.w*sub_height*image.n) + x*sub_width*image.n;

        sub_image->detect_buffer = detect_buffers[actual_thread_count];
        sub_image->data = image.data + offset;
        sub_image->w = sub_width;
        sub_image->h = sub_height;
        sub_image->n = image.n;
        sub_image->step = image.w*image.n;
        sub_image->arena = arena;
        sub_image->subx = x;
        sub_image->suby = y;


        if(pthread_create(thread, NULL, detect_faces, (void*)sub_image) == 0)
        {
            LOGI("Thread %d started (%d, %d)", i, x, y);
            actual_thread_count++;
        }
        else
        {
            LOGW("Failed to start thread");
        }

        x++;
        if(x >= cols)
        {
            x = 0;
            y++;
        }
    }

    int num_faces = 0;

    for(int i = 0; i < num_threads; ++i)
    {
        LOGI("Thread %d joined", i);
        pthread_join(threads[i], NULL);
    }

    for(int i = 0; i < actual_thread_count; ++i)
    {
        Image* sub_image = sub_images[i];
        if(sub_image && sub_image->result)
        {
            u8* ret_rects = sub_image->result;
            int offset = 0;
            int sub_faces_found = *((int*)(ret_rects));
            num_faces += sub_faces_found;
            offset += sizeof(int);

            for(int j = 0; j < sub_faces_found; ++j)
            {
                Rect* r = (Rect*)(ret_rects+offset);
                Color c = {0,255,255,255};
                //transform_draw_rect(&image, *r, c, false);
                transform_black_out(&image, *r);
                LOGI("Rect: [%u,%u,%u,%u] confidence: %u", r->x, r->y, r->w, r->h, r->confidence);
                offset += sizeof(Rect);
            }
        }
    }

    LOGI("%d faces detected", num_faces);

    reverse_rgb_order(&image);

    LOGI("Writing output file...\n");
    int step = image.w*image.n;
    int res = stbi_write_png("output/out.png", image.w, image.h, image.n, image.data, step);
    if(res == 0)
    {
        LOGE("Failed to write output");
        return 1;
    }

    return 0;
}
