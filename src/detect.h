#include "base.h"
#include "os.h"
#include "util.h"
#include "transform.h"
#include "facedetectcnn.h"

#define ENABLE_NCNN 1

#if ENABLE_NCNN
#include "ncnn/net.h"

typedef struct
{
    ncnn::Net net;
    b32 initialized;
} Model;

Model scrfd = {};

void detect_init()
{
    scrfd.net.opt.num_threads = MIN(4, settings.thread_count);

    if(scrfd.net.load_param("models/scrfd_500m.ncnn.param") != 0)
    {
        loge("Failed to load SCRFD param file.");
        return;
    }

    if(scrfd.net.load_model("models/scrfd_500m.ncnn.bin") != 0)
    {
        loge("Failed to load SCRFD bin file.");
        return;
    }

    scrfd.initialized = true;
}

void* detect_faces(void* arg)
{
    if (!scrfd.initialized)
    {
        logw("SCRFD model not initialized");
        return NULL;
    }

    Image* image = (Image *)arg;
    Arena* arena = (Arena *)image->arena;

    const int net_w = 640;
    const int net_h = 640;

    // Anchor counts: 2 anchors per location, strides 8/16/32
    // at 640x640: 80x80x2=12800, 40x40x2=3200, 20x20x2=800
    static const int strides[] = {8, 16, 32};
    static const int anchorCounts[] = {12800, 3200, 800};

    // --- Letterbox (aspect-ratio preserving scale + pad) ---
    float scale = MIN((f32)net_w / image->w, (f32)net_h / image->h);
    int scaled_w = (int)(image->w * scale);
    int scaled_h = (int)(image->h * scale);

    int pad_w = net_w - scaled_w;
    int pad_h = net_h - scaled_h;
    int padX = pad_w / 2;
    int padY = pad_h / 2;

    // --- Preprocess ---
    // image->data is RGB packed
    ncnn::Mat in_resized = ncnn::Mat::from_pixels_resize(
        image->data, ncnn::Mat::PIXEL_RGB,
        image->w, image->h, scaled_w, scaled_h);

    ncnn::Mat input;
    ncnn::copy_make_border(in_resized, input,
        padY, pad_h - padY,
        padX, pad_w - padX,
        ncnn::BORDER_CONSTANT, 0.f);

    // SCRFD normalization: maps [0,255] to [-1,1]
    const f32 mean[3] = {127.5f, 127.5f, 127.5f};
    const f32 norm[3] = {1/128.f, 1/128.f, 1/128.f};
    input.substract_mean_normalize(mean, norm);

    // --- Inference ---
    ncnn::Extractor ex = scrfd.net.create_extractor();
    ex.set_light_mode(true);

    ex.input("in0", input);

    ncnn::Mat score_8, score_16, score_32;
    ncnn::Mat bbox_8, bbox_16, bbox_32;
    ncnn::Mat kps_8, kps_16, kps_32;

    ex.extract("out0", score_8);
    ex.extract("out1", score_16);
    ex.extract("out2", score_32);
    ex.extract("out3", bbox_8);
    ex.extract("out4", bbox_16);
    ex.extract("out5", bbox_32);
    ex.extract("out6", kps_8);
    ex.extract("out7", kps_16);
    ex.extract("out8", kps_32);

    const ncnn::Mat scoreMats[] = {score_8, score_16, score_32};
    const ncnn::Mat bboxMats[]  = {bbox_8, bbox_16, bbox_32};
    const ncnn::Mat kpsMats[]   = {kps_8, kps_16, kps_32};

    f32 score_threshold = settings.confidence_threshold / 100.0f;

    // --- Decode ---
    std::vector<Box> candidates;

    for (int s = 0; s < 3; s++)
    {
        int stride = strides[s];
        int count = anchorCounts[s];
        int cols = net_w / stride; // feature map width
        int rows = net_h / stride; // feature map height

        // anchor base size matches stride
        float anchor_half = stride * 0.5f;

        // flat pointers — layout is anchor-major [count x channels]
        // for dims=2: w=channels, h=count, data is row-major
        const f32* score = (const f32*)scoreMats[s]; // [count x 1]
        const f32* bbox = (const f32*)bboxMats[s]; // [count x 4]
        const f32* kps = (const f32*)kpsMats[s]; // [count x 10]

        // 2 anchors per location, scales [1, 2]
        float anchor_scales[2] = {1.0f, 2.0f};

        for (int a = 0; a < 2; a++)
        {
            float half = anchor_half * anchor_scales[a];

            for (int i = 0; i < rows; i++)
            {
                for (int j = 0; j < cols; j++)
                {
                    // interleaved anchor index:
                    // anchor 0 for all locations, then anchor 1
                    //int idx = a * rows * cols + i * cols + j;
                    int idx = (i*cols+j)*2+a;

                    float prob = score[idx];
                    if (prob < score_threshold) continue;

                    // anchor center
                    float cx = j * stride;
                    float cy = i * stride;

                    // bbox decode: ltrb distances scaled by stride
                    float l = bbox[idx * 4 + 0] * stride;
                    float t = bbox[idx * 4 + 1] * stride;
                    float r = bbox[idx * 4 + 2] * stride;
                    float b = bbox[idx * 4 + 3] * stride;

                    // map back to original image space
                    float x1 = cx - l;
                    float y1 = cy - t;
                    float x2 = cx + r;
                    float y2 = cy + b;

                    Box box;
                    box.x = (s32)(((x1) - padX) / scale);
                    box.y = (s32)(((y1) - padY) / scale);
                    box.w = (s32)((x2 - x1) / scale);
                    box.h = (s32)((y2 - y1) / scale);
                    box.x = MAX(0, MIN(box.x, image->w - 1));
                    box.y = MAX(0, MIN(box.y, image->h - 1));
                    box.w = MAX(1, MIN(box.w, image->w - box.x));
                    box.h = MAX(1, MIN(box.h, image->h - box.y));
                    box.confidence = (s32)(prob * 100.f);
                    box.interpolated = 0;

                    // landmarks: 5 keypoints, (dx,dy) relative to anchor center
                    for (int k = 0; k < 5; k++)
                    {
                        float lx = cx + kps[idx * 10 + k * 2 + 0] * stride;
                        float ly = cy + kps[idx * 10 + k * 2 + 1] * stride;
                        box.landmarks[k].x = (s32)((lx - padX) / scale);
                        box.landmarks[k].y = (s32)((ly - padY) / scale);
                    }

                    candidates.push_back(box);
                }
            }
        }
    }

    logv("SCRFD candidates: %d", (int)candidates.size());

    // --- NMS ---
    std::sort(candidates.begin(), candidates.end(),
        [](const Box& a, const Box& b){ return a.confidence > b.confidence; });

    std::vector<Box> results;
    std::vector<bool> suppressed(candidates.size(), false);

    for (size_t i = 0; i < candidates.size(); i++)
    {
        if (suppressed[i]) continue;
        results.push_back(candidates[i]);

        f32 ax1 = (f32)candidates[i].x;
        f32 ay1 = (f32)candidates[i].y;
        f32 ax2 = (f32)(candidates[i].x + candidates[i].w);
        f32 ay2 = (f32)(candidates[i].y + candidates[i].h);
        f32 aArea = (ax2 - ax1) * (ay2 - ay1);

        for (size_t j = i + 1; j < candidates.size(); j++)
        {
            if (suppressed[j]) continue;

            f32 bx1 = (f32)candidates[j].x;
            f32 by1 = (f32)candidates[j].y;
            f32 bx2 = (f32)(candidates[j].x + candidates[j].w);
            f32 by2 = (f32)(candidates[j].y + candidates[j].h);
            f32 bArea = (bx2 - bx1) * (by2 - by1);

            f32 ix1 = MAX(ax1, bx1), iy1 = MAX(ay1, by1);
            f32 ix2 = MIN(ax2, bx2), iy2 = MIN(ay2, by2);
            f32 inter = MAX(0.f, ix2 - ix1) * MAX(0.f, iy2 - iy1);
            f32 iou = inter / (aArea + bArea - inter);

            if (iou > settings.nms_iou_threshold)
                suppressed[j] = true;
        }
    }

    s32 offset = 0;
    s32 num_faces = results.size();
    image->result = (u8*)PUSH_ARRAY(arena, u8, sizeof(s32) + num_faces+sizeof(Box));

    MemoryCopy(image->result, &num_faces, sizeof(s32));
    offset += sizeof(s32);

    for (size_t i = 0; i < results.size(); i++)
    {
        Box b = results[i];
        logv("scrfd %zu: [%d %d %d %d conf:%d]", i, b.x, b.y, b.w, b.h, b.confidence);

        Box *r = (Box*)(image->result+offset);
        MemoryCopy(image->result+offset, &results[i], sizeof(Box));
        offset += sizeof(Box);
    }


    return NULL;
}

#else


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

#endif

// Returns number of boxes
s32 process_image(Image* image,Box* ret_boxes)
{
    if(!threads) return 0;

#if !ENABLE_NCNN
    // reverse to BGR
    reverse_rgb_order(image);
#endif

    // Determine image subdivision

    s32 rows = 1;
    s32 cols = 1;

    //settings.thread_count = 1;
    u32 thread_count = 1;

    s32 sub_width  = ceil(image->w / cols);
    s32 sub_height = ceil(image->h / rows);

    logi("Image sub-size: (%d, %d), config: %dx%d", sub_width, sub_height, rows, cols);

    Temp scratch = scratch_begin();

    Image** sub_images = (Image**)calloc(thread_count, sizeof(Image*));
    u8 *detect_buffers = (u8 *)PUSH_ARRAY(scratch.arena, u8, 0x9000 * thread_count);

    for(s32 i = 0; i < thread_count; ++i)
    {
        arena_reset(thread_arenas[i]);
        sub_images[i] = (Image*)PUSH_ONE(thread_arenas[i], Image);
    }

    s32 actual_thread_count = 0;
    s32 x = 0;
    s32 y = 0;

    logi("Detecting faces... (threads: %d)", thread_count);

    const f32 padding_factor = 0.1;
    s32 padding = MAX(sub_width, sub_height)*padding_factor;

    timer_begin(&timer);

    for(s32 i = 0; i < thread_count; ++i)
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

    for(s32 i = 0; i < thread_count; ++i)
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

#if !ENABLE_NCNN
    reverse_rgb_order(image);
#endif

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

