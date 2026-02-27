#include "base.h"
#include "os.h"
#include "util.h"
#include "transform.h"
#include "ncnn/net.h"

typedef struct
{
    ncnn::Net net;
    b32 initialized;
} Model;

Model model_face   = {};
Model model_person = {};

void detect_init()
{
    model_face.net.opt.num_threads = MIN(4, settings.thread_count);

    if(model_face.net.load_param("models/scrfd_500m_gnkps.ncnn.param") != 0)
    {
        loge("Failed to load face param file.");
        return;
    }

    if(model_face.net.load_model("models/scrfd_500m_gnkps.ncnn.bin") != 0)
    {
        loge("Failed to load face bin file.");
        return;
    }

    model_face.initialized = true;

    model_person.net.opt.num_threads = MIN(4, settings.thread_count);

    if(model_person.net.load_param("models/scrfd_person_2.5g.ncnn.param") != 0)
    {
        loge("Failed to load person param file.");
        return;
    }

    if(model_person.net.load_model("models/scrfd_person_2.5g.ncnn.bin") != 0)
    {
        loge("Failed to load person bin file.");
        return;
    }

    model_person.initialized = true;
}


void *detect_faces(void *arg)
{
    if (!model_face.initialized)
    {
        logw("Face Model model not initialized");
        return NULL;
    }

    Image* image = (Image *)arg;
    Arena* arena = (Arena *)image->arena;

    const s32 net_w = 640;
    const s32 net_h = 640;

    // Anchor counts: 2 anchors per location, strides 8/16/32
    // at 640x640: 80x80x2=12800, 40x40x2=3200, 20x20x2=800
    static const s32 strides[] = {8, 16, 32};
    static const s32 anchorCounts[] = {12800, 3200, 800};

    // --- Letterbox (aspect-ratio preserving scale + pad) ---
    f32 scale = MIN((f32)net_w / image->w, (f32)net_h / image->h);
    s32 scaled_w = (s32)(image->w * scale);
    s32 scaled_h = (s32)(image->h * scale);

    s32 pad_w = net_w - scaled_w;
    s32 pad_h = net_h - scaled_h;
    s32 padX = pad_w / 2;
    s32 padY = pad_h / 2;

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

    // Face Model normalization: maps [0,255] to [-1,1]
    const f32 mean[3] = {127.5f, 127.5f, 127.5f};
    const f32 norm[3] = {1/128.f, 1/128.f, 1/128.f};
    input.substract_mean_normalize(mean, norm);

    // --- Inference ---
    ncnn::Extractor ex = model_face.net.create_extractor();
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

    f32 score_threshold = settings.confidence_threshold;

    // --- Decode ---
    std::vector<Box> candidates;

    for (s32 s = 0; s < 3; s++)
    {
        s32 stride = strides[s];
        s32 count = anchorCounts[s];
        s32 cols = net_w / stride; // feature map width
        s32 rows = net_h / stride; // feature map height

        // anchor base size matches stride
        f32 anchor_half = stride * 0.5f;

        // flat pointers — layout is anchor-major [count x channels]
        // for dims=2: w=channels, h=count, data is row-major
        const f32* score = (const f32*)scoreMats[s]; // [count x 1]
        const f32* bbox = (const f32*)bboxMats[s]; // [count x 4]
        const f32* kps = (const f32*)kpsMats[s]; // [count x 10]

        // 2 anchors per location, scales [1, 2]
        f32 anchor_scales[2] = {1.0f, 2.0f};

        for (s32 a = 0; a < 2; a++)
        {
            f32 half = anchor_half * anchor_scales[a];

            for (s32 i = 0; i < rows; i++)
            {
                for (s32 j = 0; j < cols; j++)
                {
                    // interleaved anchor index:
                    // anchor 0 for all locations, then anchor 1
                    s32 idx = (i*cols+j)*2+a;

                    f32 prob = score[idx];
                    if (prob < score_threshold) continue;

                    // anchor center
                    f32 cx = j * stride;
                    f32 cy = i * stride;

                    // bbox decode: ltrb distances scaled by stride
                    f32 l = bbox[idx * 4 + 0] * stride;
                    f32 t = bbox[idx * 4 + 1] * stride;
                    f32 r = bbox[idx * 4 + 2] * stride;
                    f32 b = bbox[idx * 4 + 3] * stride;

                    // map back to original image space
                    f32 x1 = cx - l;
                    f32 y1 = cy - t;
                    f32 x2 = cx + r;
                    f32 y2 = cy + b;

                    Box box;
                    box.x = (s32)(((x1) - padX) / scale);
                    box.y = (s32)(((y1) - padY) / scale);
                    box.w = (s32)((x2 - x1) / scale);
                    box.h = (s32)((y2 - y1) / scale);
                    box.x = MAX(0, MIN(box.x, image->w - 1));
                    box.y = MAX(0, MIN(box.y, image->h - 1));
                    box.w = MAX(1, MIN(box.w, image->w - box.x));
                    box.h = MAX(1, MIN(box.h, image->h - box.y));
                    box.confidence = (s32)(prob * 100);
                    box.interpolated = 0;

                    // landmarks: 5 keypoints, (dx,dy) relative to anchor center
                    for (s32 k = 0; k < 5; k++)
                    {
                        f32 lx = cx + kps[idx * 10 + k * 2 + 0] * stride;
                        f32 ly = cy + kps[idx * 10 + k * 2 + 1] * stride;
                        box.landmarks[k].x = (s32)((lx - padX) / scale);
                        box.landmarks[k].y = (s32)((ly - padY) / scale);
                    }

                    candidates.push_back(box);
                }
            }
        }
    }

    logv("Face Model candidates: %d", (s32)candidates.size());

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
        logv("Face Model %zu: [%d %d %d %d conf:%d]", i, b.x, b.y, b.w, b.h, b.confidence);

        Box *r = (Box*)(image->result+offset);
        MemoryCopy(image->result+offset, &results[i], sizeof(Box));
        offset += sizeof(Box);
    }


    return NULL;
}

void* detect_persons(void* arg)
{
    if (!model_person.initialized)
    {
        logw("Person model not initialized");
        return NULL;
    }

    Image* image = (Image *)arg;
    Arena* arena = (Arena *)image->arena;

    const int net_w = 640;
    const int net_h = 640;

    static const int strides[]      = {8,    16,   32,  64,  128};
    static const int anchorCounts[] = {6400, 1600, 400, 100, 25};

    // --- Letterbox ---
    float scale  = MIN((f32)net_w / image->w, (f32)net_h / image->h);
    int scaled_w = (int)(image->w * scale);
    int scaled_h = (int)(image->h * scale);
    int pad_w    = net_w - scaled_w;
    int pad_h    = net_h - scaled_h;
    int padX     = pad_w / 2;
    int padY     = pad_h / 2;

    // --- Preprocess ---
    ncnn::Mat in_resized = ncnn::Mat::from_pixels_resize(
        image->data, ncnn::Mat::PIXEL_RGB,
        image->w, image->h, scaled_w, scaled_h);

    ncnn::Mat input;
    ncnn::copy_make_border(in_resized, input,
        padY, pad_h - padY,
        padX, pad_w - padX,
        ncnn::BORDER_CONSTANT, 0.f);

    const f32 mean[3] = {127.5f, 127.5f, 127.5f};
    const f32 norm[3] = {1/128.f, 1/128.f, 1/128.f};
    input.substract_mean_normalize(mean, norm);

    // --- Inference ---
    ncnn::Extractor ex = model_person.net.create_extractor();
    ex.set_light_mode(true);

    ex.input("in0", input);

    ncnn::Mat score_8,   score_16,  score_32,  score_64,  score_128;
    ncnn::Mat bbox_8,    bbox_16,   bbox_32,   bbox_64,   bbox_128;
    ncnn::Mat kps_8,     kps_16,    kps_32,    kps_64,    kps_128;

    ex.extract("out0",  score_8);
    ex.extract("out1",  score_16);
    ex.extract("out2",  score_32);
    ex.extract("out3",  score_64);
    ex.extract("out4",  score_128);
    ex.extract("out5",  bbox_8);
    ex.extract("out6",  bbox_16);
    ex.extract("out7",  bbox_32);
    ex.extract("out8",  bbox_64);
    ex.extract("out9",  bbox_128);
    ex.extract("out10", kps_8);
    ex.extract("out11", kps_16);
    ex.extract("out12", kps_32);
    ex.extract("out13", kps_64);
    ex.extract("out14", kps_128);

    const ncnn::Mat scoreMats[] = {score_8,  score_16,  score_32,  score_64,  score_128};
    const ncnn::Mat bboxMats[]  = {bbox_8,   bbox_16,   bbox_32,   bbox_64,   bbox_128};
    const ncnn::Mat kpsMats[]   = {kps_8,    kps_16,    kps_32,    kps_64,    kps_128};

    f32 score_threshold = 0.45; //settings.confidence_threshold;

    // --- Decode ---
    std::vector<Box> candidates;

    for (int s = 0; s < 5; s++)
    {
        int stride = strides[s];
        int count  = anchorCounts[s];
        int cols   = net_w / stride;
        int rows   = net_h / stride;

        const f32* score = (const f32*)scoreMats[s];
        const f32* bbox  = (const f32*)bboxMats[s];
        const f32* kps   = (const f32*)kpsMats[s];

        for (int i = 0; i < rows; i++)
        {
            for (int j = 0; j < cols; j++)
            {
                int idx = i * cols + j;

                float prob = score[idx];
                if (prob < score_threshold) continue;

                float cx = j * stride;
                float cy = i * stride;

                float l = bbox[idx * 4 + 0] * stride;
                float t = bbox[idx * 4 + 1] * stride;
                float r = bbox[idx * 4 + 2] * stride;
                float b = bbox[idx * 4 + 3] * stride;

                Box box;
                box.x = (s32)(((cx - l) - padX) / scale);
                box.y = (s32)(((cy - t) - padY) / scale);
                box.w = (s32)((l + r) / scale);
                box.h = (s32)((t + b) / scale);
                box.x = MAX(0, MIN(box.x, image->w - 1));
                box.y = MAX(0, MIN(box.y, image->h - 1));
                box.w = MAX(1, MIN(box.w, image->w - box.x));
                box.h = MAX(1, MIN(box.h, image->h - box.y));
                box.confidence  = (s32)(prob * 100.f);
                box.interpolated = 0;

                for (int k = 0; k < 5; k++)
                {
                    box.landmarks[k].x = (s32)((cx + kps[idx * 10 + k*2+0] * stride - padX) / scale);
                    box.landmarks[k].y = (s32)((cy + kps[idx * 10 + k*2+1] * stride - padY) / scale);
                }

                candidates.push_back(box);
            }
        }
    }

    logv("Person model candidates: %d", (int)candidates.size());

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
            f32 iou   = inter / (aArea + bArea - inter);

            if (iou > settings.nms_iou_threshold)
                suppressed[j] = true;
        }
    }

    s32 offset = 0;
    s32 num_persons = results.size();
    image->result = (u8*)PUSH_ARRAY(arena, u8, sizeof(s32) + num_persons+sizeof(Box));

    MemoryCopy(image->result, &num_persons, sizeof(s32));
    offset += sizeof(s32);

    for (size_t i = 0; i < results.size(); i++)
    {
        Box b = results[i];
        logv("person %zu: [%d %d %d %d conf:%d]", i, b.x, b.y, b.w, b.h, b.confidence);

        Box *r = (Box*)(image->result+offset);
        MemoryCopy(image->result+offset, &results[i], sizeof(Box));
        offset += sizeof(Box);
    }

    return NULL;
}

void *detect_run(void *arg)
{
    for(int i = 0; i < settings.class_count; ++i)
    {
        DetectClass c = settings.classes[i];

        switch(c)
        {
            case CLASS_FACE:   detect_faces(arg);   break;
            case CLASS_PERSON: detect_persons(arg); break;

            default: break;
        }
    }
    return NULL;
}

// Returns number of boxes
s32 process_image(Image* image, Box* ret_boxes)
{
    if(!threads) return 0;

    u8 detect_buffer[0x9000];
    MemoryZero(detect_buffer, 0x9000);

    logi("Detecting faces...");

    timer_begin(&timer);

    image->detect_buffer = detect_buffer;

    detect_run((void *)image);

    f64 detection_time = timer_get_elapsed(&timer);
    logi("detection time: %.3f ms", detection_time*1000.0f);

    Box total_boxes[1024] = {};
    s32 num_faces = 0;

    // collect face box results
    if(image->result)
    {
        u8* ret_boxes = image->result;
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

    s32 ret_boxes_count = 0;

    for(s32 i = 0; i < num_faces; ++i)
    {
        Box *ret_box = &ret_boxes[ret_boxes_count];
        Box *box = &total_boxes[i];

        MemoryCopy(ret_box, box, sizeof(Box));
        ret_boxes_count++;
    }

    return ret_boxes_count;
}

