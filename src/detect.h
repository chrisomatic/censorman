#include "base.h"
#include "platform.h"
#include "transform.h"
#include "util.h"
#include "facedetectcnn.h"

void detect_init()
{
    facedetect_init(); // copies model data to be used
}

void *detect_faces(void* arg)
{
    Image *image = (Image *)arg;
    Arena *arena = (Arena *)image->arena;

    int *results = facedetect_cnn(image->detect_buffer,image->data,image->w,image->h,image->step, (float)(settings.confidence_threshold / 100.0f));
    int num_faces = (results ? *results : 0);

    image->result = (u8*)PUSH_ARRAY(arena, u8, sizeof(int) + num_faces+sizeof(Rect));

    int offset = 0;

    memcpy(image->result, &num_faces, sizeof(int));
    offset += sizeof(int);

    for(int i = 0; i < num_faces; ++i)
    {
        short *p = ((short*)(results+1)) + 16*i;

        Rect *r = (Rect*)(image->result+offset);

        r->confidence = p[0];
        r->x = p[1] + (image->subx*image->w);
        r->y = p[2] + (image->suby*image->h);
        r->w = p[3];
        r->h = p[4];

        r->landmarks[0].x = p[5] + (image->subx*image->w);
        r->landmarks[0].y = p[6] + (image->suby*image->h);
        r->landmarks[1].x = p[7] + (image->subx*image->w);
        r->landmarks[1].y = p[8] + (image->suby*image->h);
        r->landmarks[2].x = p[9] + (image->subx*image->w);
        r->landmarks[2].y = p[10] + (image->suby*image->h);
        r->landmarks[3].x = p[11] + (image->subx*image->w);
        r->landmarks[3].y = p[12] + (image->suby*image->h);
        r->landmarks[4].x = p[13] + (image->subx*image->w);
        r->landmarks[4].y = p[14] + (image->suby*image->h);

        offset += sizeof(Rect);
    }

    return NULL;
}

// Returns number of rects
int process_image(Image* image,Rect* ret_rects)
{
    if(!threads) return 0;

    reverse_rgb_order(image);

    // Determine image subdivision

    int rows = 0;
    int cols = 0;

    switch(settings.thread_count)
    {
        case 1:  rows = 1; cols = 1;  break;
        case 2:  rows = 1; cols = 2;  break;
        case 3:  rows = 1; cols = 3;  break;
        case 4:  rows = 2; cols = 2;  break;
        case 5:  rows = 1; cols = 5;  break;
        case 6:  rows = 2; cols = 3;  break;
        case 7:  rows = 1; cols = 7;  break;
        case 8:  rows = 2; cols = 4;  break;
        case 9:  rows = 3; cols = 3;  break;
        case 10: rows = 2; cols = 5;  break;
        case 11: rows = 1; cols = 11; break;
        case 12: rows = 3; cols = 4;  break;
        case 13: rows = 1; cols = 13; break;
        case 14: rows = 2; cols = 7;  break;
        case 15: rows = 3; cols = 5;  break;
        case 16: rows = 4; cols = 4;  break;
        default: rows = 1; cols = settings.thread_count;
            break;
    }

    bool is_vert = (image->h >= image->w);

    if(is_vert)
    {
        // swap rows/cols
        int tmp = rows;
        rows = cols;
        cols = tmp;
    }

    int sub_width  = ceil(image->w / cols);
    int sub_height = ceil(image->h / rows);

    LOGI("Image sub-size: (%d, %d), config: %dx%d", sub_width, sub_height, rows, cols);

    Temp scratch = scratch_begin();

    Image** sub_images = (Image**)calloc(settings.thread_count, sizeof(Image*));
    u8 *detect_buffers = (u8 *)PUSH_ARRAY(scratch.arena, u8, 0x9000 * settings.thread_count);

    for(int i = 0; i < settings.thread_count; ++i)
    {
        arena_reset(thread_arenas[i]);
        sub_images[i] = (Image*)PUSH_ONE(thread_arenas[i], Image);
    }

    int actual_thread_count = 0;
    int x = 0;
    int y = 0;

    LOGI("Detecting faces... (threads: %d)", settings.thread_count);

    const float padding_factor = 0.1;
    int padding = MAX(sub_width, sub_height)*padding_factor;

    timer_begin(&timer);

    for(int i = 0; i < settings.thread_count; ++i)
    {
        Arena* arena = thread_arenas[actual_thread_count];
        Image* sub_image = sub_images[actual_thread_count];

        // calculate offset into base image
        int offset = (y*image->w*sub_height*image->n) + x*sub_width*image->n;

        sub_image->detect_buffer = (detect_buffers + (0x9000 * actual_thread_count));
        sub_image->data = image->data + offset;
        sub_image->w = sub_width;
        sub_image->h = sub_height;
        sub_image->n = image->n;
        sub_image->step = image->w*image->n;
        sub_image->arena = arena;
        sub_image->subx = x;
        sub_image->suby = y;

        if(thread_create(&threads[actual_thread_count], detect_faces, (void*)sub_image) == 0)
        {
            //LOGI("Thread %d started (%d, %d)", i, x, y);
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

    for(int i = 0; i < settings.thread_count; ++i)
    {
        //LOGI("Thread %d joined", i);
        thread_join(threads[i]);
    }

    double detection_time = timer_get_elapsed(&timer);
    LOGI("detection time: %.3f ms", detection_time*1000.0f);

    Rect total_rects[1024] = {};
    int num_faces = 0;

    // collect face box results
    for(int i = 0; i < actual_thread_count; ++i)
    {
        Image* sub_image = sub_images[i];
        if(sub_image && sub_image->result)
        {
            u8* ret_rects = sub_image->result;
            int offset = 0;
            int sub_faces_found = *((int*)(ret_rects));
            offset += sizeof(int);

            for(int j = 0; j < sub_faces_found; ++j)
            {
                Rect* r = (Rect*)(ret_rects+offset);
                if(r->x >= image->w || r->y >= image->h)
                    continue;

                if(r->x + r->w > image->w) r->w = image->w - r->x - 1;
                if(r->y + r->h > image->h) r->h = image->h - r->y - 1;

                memcpy(&total_rects[num_faces],r,sizeof(Rect));
                offset += sizeof(Rect);
                num_faces++;
            }
        }
    }

    reverse_rgb_order(image);

    // sort and filter out detected boxes
    util_sort_rects(num_faces, total_rects, false);

    // NMS (Non-Maximum Suppression)
    // Conlidate detection regions

    bool removed_rects[1024] = {};
    int num_removed = 0;

    for(int i = 0; i < num_faces; ++i)
    {
        if(removed_rects[i])
            continue;

        Rect* a = &total_rects[i];

        for(int j = i+1; j < num_faces; ++j)
        {
            Rect* b = &total_rects[j];
            float iou = calc_iou(a,b);

            if(iou > settings.nms_iou_threshold)
            {
                // remove the less confidence box
                int idx = (a->confidence < b->confidence ? i : j);
                removed_rects[idx] = true;
                num_removed++;
            }
        }
    }

    LOGI("NMS removed %d rects", num_removed);

    int ret_rects_count = 0;
    for(int i  = 0; i < num_faces; ++i)
    {
        if(removed_rects[i])
            continue;

        memcpy(&ret_rects[ret_rects_count++], &total_rects[i], sizeof(Rect));
    }

    scratch_end(scratch);

    return ret_rects_count;
}

