#include "base.h"
#include "os.h"
#include "util.h"
#include "transform.h"
#include "facedetectcnn.h"

#define ENABLE_NCNN 0

#if ENABLE_NCNN
#include "ncnn/net.h"

typedef struct
{
    ncnn::Net net;
    b32 initialized;
} Model;

Model yunet = {};

void detect_init2()
{
    if(yunet.net.load_param("models/yunet.param") != 0)
    {
        loge("Failed to load YuNET param file.");
        return;
    }

    if(yunet.net.load_model("models/yunet.bin") != 0)
    {
        loge("Failed to load YuNET bin file.");
        return;
    }

    yunet.initialized = true;
}

void *detect_faces2(void *arg)
{
    Image *image = (Image *)arg;

    ncnn::Mat in = ncnn::Mat::from_pixels_resize(image->data, ncnn::Mat::PIXEL_BGR2RGB, image->w, image->h, 320, 320);

    // const f32 mean_vals[3] = { 103.94f, 116.78f, 123.68f };
    // const f32 norm_vals[3] = { 0.017f, 0.017f, 0.017f };
    const f32 mean_vals[3] = {0.f, 0.f, 0.f};
    const f32 norm_vals[3] = {1.f, 1.f, 1.f};
    in.substract_mean_normalize(mean_vals, norm_vals);

    ncnn::Extractor ex = yunet.net.create_extractor();
    ex.input("in0", in); // 'input' is the input node name in param file

    ncnn::Mat score_map;
    ncnn::Mat location_map;
    ncnn::Mat landmarks_map;

    // Extract outputs (names depend on your param file)
    ex.extract("out0", score_map);
    ex.extract("out1", location_map);
    ex.extract("out2", landmarks_map);

    return NULL;
}
#endif

void detect_init()
{
    facedetect_init(); // copies model data to be used
}

void *detect_faces(void* arg)
{
    Image *image = (Image *)arg;
    Arena *arena = (Arena *)image->arena;

    s32 *results = facedetect_cnn(image->detect_buffer,image->data,image->w,image->h,image->step, (f32)(settings.confidence_threshold / 100.0f));
    s32 num_faces = (results ? *results : 0);

    image->result = (u8*)PUSH_ARRAY(arena, u8, sizeof(s32) + num_faces+sizeof(Box));

    s32 offset = 0;

    memcpy(image->result, &num_faces, sizeof(s32));
    offset += sizeof(s32);

    for(s32 i = 0; i < num_faces; ++i)
    {
        short *p = ((short*)(results+1)) + 16*i;

        Box *r = (Box*)(image->result+offset);

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

        offset += sizeof(Box);
    }

    return NULL;
}

// Returns number of boxes
s32 process_image(Image* image,Box* ret_boxes)
{
    if(!threads) return 0;

    reverse_rgb_order(image);

    // Determine image subdivision

    s32 rows = 0;
    s32 cols = 0;

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

    b32 is_vert = (image->h >= image->w);

    if(is_vert)
    {
        // swap rows/cols
        s32 tmp = rows;
        rows = cols;
        cols = tmp;
    }

    s32 sub_width  = ceil(image->w / cols);
    s32 sub_height = ceil(image->h / rows);

    logi("Image sub-size: (%d, %d), config: %dx%d", sub_width, sub_height, rows, cols);

    Temp scratch = scratch_begin();

    Image** sub_images = (Image**)calloc(settings.thread_count, sizeof(Image*));
    u8 *detect_buffers = (u8 *)PUSH_ARRAY(scratch.arena, u8, 0x9000 * settings.thread_count);

    for(s32 i = 0; i < settings.thread_count; ++i)
    {
        arena_reset(thread_arenas[i]);
        sub_images[i] = (Image*)PUSH_ONE(thread_arenas[i], Image);
    }

    s32 actual_thread_count = 0;
    s32 x = 0;
    s32 y = 0;

    logi("Detecting faces... (threads: %d)", settings.thread_count);

    const f32 padding_factor = 0.1;
    s32 padding = MAX(sub_width, sub_height)*padding_factor;

    timer_begin(&timer);

    for(s32 i = 0; i < settings.thread_count; ++i)
    {
        Arena* arena = thread_arenas[actual_thread_count];
        Image* sub_image = sub_images[actual_thread_count];

        // calculate offset into base image
        s32 offset = (y*image->w*sub_height*image->n) + x*sub_width*image->n;

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
            //logi("Thread %d started (%d, %d)", i, x, y);
            actual_thread_count++;
        }
        else
        {
            logw("Failed to start thread");
        }

        x++;
        if(x >= cols)
        {
            x = 0;
            y++;
        }
    }

    for(s32 i = 0; i < settings.thread_count; ++i)
    {
        //logi("Thread %d joined", i);
        thread_join(threads[i]);
    }

    f64 detection_time = timer_get_elapsed(&timer);
    logi("detection time: %.3f ms", detection_time*1000.0f);

    Box total_boxes[1024] = {};
    s32 num_faces = 0;

    // collect face box results
    for(s32 i = 0; i < actual_thread_count; ++i)
    {
        Image* sub_image = sub_images[i];
        if(sub_image && sub_image->result)
        {
            u8* ret_boxes = sub_image->result;
            s32 offset = 0;
            s32 sub_faces_found = *((s32*)(ret_boxes));
            offset += sizeof(s32);

            for(s32 j = 0; j < sub_faces_found; ++j)
            {
                Box* r = (Box*)(ret_boxes+offset);
                if(r->x >= image->w || r->y >= image->h)
                    continue;

                if(r->x + r->w > image->w) r->w = image->w - r->x - 1;
                if(r->y + r->h > image->h) r->h = image->h - r->y - 1;

                memcpy(&total_boxes[num_faces],r,sizeof(Box));
                offset += sizeof(Box);
                num_faces++;
            }
        }
    }

    reverse_rgb_order(image);

    s32 ret_boxes_count = 0;
    for(s32 i = 0; i < num_faces; ++i)
    {
        Box *ret_box = &ret_boxes[ret_boxes_count];
        Box *box = &total_boxes[i];

        MemoryCopy(ret_box, box, sizeof(Box));

        ret_boxes_count++;
    }

    scratch_end(scratch);

    return ret_boxes_count;
}

