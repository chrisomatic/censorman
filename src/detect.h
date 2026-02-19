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

Model yunet = {};

static const int strides[] = {8, 16, 32};
static const int anchorCounts[] = {6400, 1600, 400}; // per stride

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

    yunet.net.opt.num_threads = 4;
    yunet.initialized = true;
}

void *detect_faces2(void *arg)
{
    Image *image = (Image *)arg;
    Arena *arena = (Arena *)image->arena;

    const u32 net_w = 640;
    const u32 net_h = 640;

    const f32 score_threshold = settings.confidence_threshold / 100.0;

    ncnn::Mat input = ncnn::Mat::from_pixels_resize(image->data, ncnn::Mat::PIXEL_BGR,
        image->w, image->h, net_w, net_h);

    const f32 norm[3] = {1/255.f, 1/255.f, 1/255.f};
    input.substract_mean_normalize(nullptr, norm);

    // --- Inference ---
    ncnn::Extractor ex = yunet.net.create_extractor();
    ex.set_light_mode(true);

    ex.input("in0", input);

    ncnn::Mat out0, out1, out2;   // cls
    ncnn::Mat out3, out4, out5;   // obj
    ncnn::Mat out6, out7, out8;   // bbox
    ncnn::Mat out9, out10, out11; // landmarks

    ex.extract("out0", out0);
    ex.extract("out1", out1);
    ex.extract("out2", out2);
    ex.extract("out3", out3);
    ex.extract("out4", out4);
    ex.extract("out5", out5);
    ex.extract("out6", out6);
    ex.extract("out7", out7);
    ex.extract("out8", out8);
    ex.extract("out9", out9);
    ex.extract("out10", out10);
    ex.extract("out11", out11);

    const ncnn::Mat clsMats[]  = {out0, out1, out2};
    const ncnn::Mat objMats[]  = {out3, out4, out5};
    const ncnn::Mat bboxMats[] = {out6, out7, out8};
    const ncnn::Mat lmkMats[]  = {out9, out10, out11};

    // --- Decode ---
    // YuNet uses SimOTA-style decoding: face_score = cls * obj (both already sigmoid'd)
    // bbox is in the "distance-to-boundary" (ltrb) format scaled by stride
    std::vector<Box> candidates;

    for (int s = 0; s < 3; s++)
    {
        s32 stride = strides[s];
        s32 count = anchorCounts[s];

        // Grid dimensions at this stride
        s32 cols = net_w / stride;
        s32 rows = net_h / stride;

        const f32* cls  = clsMats[s];
        const f32* obj  = objMats[s];
        const f32* bbox = bboxMats[s]; // shape: [4, count] flattened row-major
        const f32* lmk  = lmkMats[s]; // shape: [10, count]

        for (s32 i = 0; i < count; ++i)
        {
            f32 score = cls[i] * obj[i];
            if (score < score_threshold) continue;

            // Anchor center
            s32 row = i / cols;
            s32 col = i % cols;
            f32 cx = (col + 0.5f) * stride;
            f32 cy = (row + 0.5f) * stride;

            // ltrb distances, scaled by stride
            f32 l = bbox[0 * count + i] * stride;
            f32 t = bbox[1 * count + i] * stride;
            f32 r = bbox[2 * count + i] * stride;
            f32 b = bbox[3 * count + i] * stride;

            Box box;
            box.x = cx - l;
            box.y = cy - t;
            box.w = (cx + r) - box.x;
            box.h = (cy + b) - box.y;
            box.confidence = score;

            // Landmarks: 5 keypoints, each (dx, dy) relative to anchor, scaled by stride
            for (s32 k = 0; k < 5; ++k)
            {
                box.landmarks[k].x = cx + lmk[(k*2 + 0) * count + i] * stride;
                box.landmarks[k].y = cy + lmk[(k*2 + 1) * count + i] * stride;
            }

            candidates.push_back(box);
        }
    }

    // --- NMS ---
    // Sort by score descending
    std::sort(candidates.begin(), candidates.end(),
        [](const Box& a, const Box& b){ return a.confidence > b.confidence; });

    std::vector<Box> results;
    std::vector<bool> suppressed(candidates.size(), false);

    for (size_t i = 0; i < candidates.size(); i++)
    {
        if (suppressed[i]) continue;
        results.push_back(candidates[i]);

        f32 ax1 = candidates[i].x;
        f32 ay1 = candidates[i].y;
        f32 ax2 = candidates[i].x + candidates[i].w;
        f32 ay2 = candidates[i].y + candidates[i].h;

        f32 aArea = (ax2 - ax1) * (ay2 - ay1);

        for (size_t j = i + 1; j < candidates.size(); j++)
        {
            if (suppressed[j]) continue;

            f32 bx1 = candidates[j].x;
            f32 by1 = candidates[j].y;
            f32 bx2 = candidates[j].x + candidates[j].w;
            f32 by2 = candidates[j].y + candidates[j].h;

            f32 bArea = (bx2 - bx1) * (by2 - by1);

            f32 ix1 = std::max(ax1, bx1), iy1 = std::max(ay1, by1);
            f32 ix2 = std::min(ax2, bx2), iy2 = std::min(ay2, by2);
            f32 inter = std::max(0.f, ix2-ix1) * std::max(0.f, iy2-iy1);
            f32 iou = inter / (aArea + bArea - inter);

            if (iou > settings.nms_iou_threshold)
                suppressed[j] = true;
        }
    }

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

    // u32 thread_count = settings.thread_count;
    u32 thread_count = 1;

    switch(thread_count)
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
        default: rows = 1; cols = thread_count;
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

