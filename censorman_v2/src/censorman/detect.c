
static Model model_face    = {0};
static Model model_person = {0};

extern void ncnn_net_set_lightmode(ncnn_net_t net, int enable);

static void model_init(Model *model)
{
    model->net = ncnn_net_create();

    model->net_w = 640;
    model->net_h = 640;

    ncnn_net_set_lightmode(model->net, 1);
    ncnn_option_t opt = ncnn_net_get_option(model->net);
    ncnn_option_set_num_threads(opt, 4);
    ncnn_net_set_option(model->net, opt);
}

b32 detect_init(void)
{
    if(!model_face.initialized)
    {
        model_init(&model_face);

        int param_ret = ncnn_net_load_param(model_face.net, "models/scrfd_500m_gnkps.ncnn.param");
        int model_ret = ncnn_net_load_model(model_face.net, "models/scrfd_500m_gnkps.ncnn.bin");

        model_face.initialized = (param_ret == 0 && model_ret == 0);
    }

    if(!model_person.initialized)
    {
        model_init(&model_person);

        int param_ret = ncnn_net_load_param(model_person.net, "models/scrfd_person_2.5g.ncnn.param");
        int model_ret = ncnn_net_load_model(model_person.net, "models/scrfd_person_2.5g.ncnn.bin");

        model_person.initialized = (param_ret == 0 && model_ret == 0);
    }

    if(!model_face.initialized || !model_person.initialized)
    {
        loge("Failed with model loading. Loaded [ Face: %s, Person: %s ]", STR_BOOL(model_face.initialized), STR_BOOL(model_person.initialized));
        return false;
    }

    return true;
}

void *detect(void *args)
{
    DetectArgs *detect_args = (DetectArgs *)args;

    Image *image       = detect_args->image;
    List  *total_boxes = detect_args->boxes;

    stopwatch_begin(image->stopwatch, S("detect"));

    DetectType type = detect_args->type;

    List new_boxes  = {0};

    Temp scratch = scratch_begin();

    switch(type)
    {
        case DETECT_TYPE_FACE:
        {
            new_boxes = detect_faces(scratch.arena, image);
        } break;
        case DETECT_TYPE_PERSON:
        {
            new_boxes = detect_persons(scratch.arena, image);
        } break;
        case DETECT_TYPE_LICENSE_PLATE:
        {
            logd("TODO: License Plate Model");
        } break;
        case DETECT_TYPE_DOCUMENT:
        {
            logd("TODO: Document Model");
        } break;
        default:
            break;
    }

    // add new boxes to total list
    list_add_list(total_boxes, &new_boxes);

    scratch_end(scratch);

    stopwatch_end(image->stopwatch, S("detect"));

    return NULL;
}

s32 box_compare(void *a, void *b)
{
    Box *box_a = (Box *)a;
    Box *box_b = (Box *)b;

    s32 result =   (box_a->confidence < box_b->confidence) 
                 - (box_a->confidence > box_b->confidence);

    return result;
}

List non_maximum_suppression(List boxes, f32 iou_threshold)
{
    // sort boxes

    ListArray boxes_arr = list_to_array(&boxes);
    list_array_sort(&boxes_arr, box_compare);

    logv("Boxes array count: %u", boxes_arr.count);
    for(int i = 0; i < boxes_arr.count; ++i)
    {
        Box *b = (Box *)(boxes_arr.items + i*sizeof(Box));
        box_print(b);
    }

    // suppress

    List boxes_curated = list_create(boxes.arena, sizeof(Box));
    b8 *suppressed = PUSH_ARRAY(boxes.arena, b8, boxes_arr.count);

    for(u64 i = 0; i < boxes_arr.count; ++i)
    {
        if(suppressed[i]) continue;

        Box *box_a = (Box *)(boxes_arr.items + i*sizeof(Box));
        list_add(&boxes_curated, box_a);

        u32 ax1 = box_a->x;
        u32 ay1 = box_a->y;
        u32 ax2 = box_a->x + box_a->w;
        u32 ay2 = box_a->y + box_a->h;

        f64 a_area = box_a->w * box_a->h;

        for(u64 j = i + 1; j < boxes_arr.count; ++j)
        {
            if(suppressed[j]) continue;

            Box *box_b = (Box *)(boxes_arr.items + j*sizeof(Box));

            u32 bx1 = box_b->x;
            u32 by1 = box_b->y;
            u32 bx2 = box_b->x + box_b->w;
            u32 by2 = box_b->y + box_b->h;

            f64 b_area = box_b->w * box_b->h;

            s32 ix1 = MAX(ax1, bx1);
            s32 iy1 = MAX(ay1, by1);
            s32 ix2 = MIN(ax2, bx2);
            s32 iy2 = MIN(ay2, by2);

            s32 inter_w = MAX(0, ix2 - ix1);
            s32 inter_h = MAX(0, iy2 - iy1);
            f64 inter = (f64)(inter_w * inter_h);
            f64 iou = inter / (a_area + b_area - inter);

            if (iou > iou_threshold)
                suppressed[j] = true;
        }
    }

    logv("Boxes count after NMS: %u", boxes_curated.count);

    return boxes_curated;
}

List detect_faces(Arena *arena, Image *image)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->w, image->h, image->w*3, 0);
    
    // Maps [0,255] --> [-1,1]

    const f32 mean[] = {127.5, 127.5, 127.5};
    const f32 norm[] = {1.0/128.0, 1.0/128.0, 1.0/128.0};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_extractor_t ex = ncnn_extractor_create(model_face.net);

    ncnn_extractor_input(ex, "in0", input);

    ncnn_mat_t score_8;
    ncnn_mat_t score_16;
    ncnn_mat_t score_32;
    ncnn_mat_t bbox_8;
    ncnn_mat_t bbox_16;
    ncnn_mat_t bbox_32;
    ncnn_mat_t kps_8;
    ncnn_mat_t kps_16;
    ncnn_mat_t kps_32;

    ncnn_extractor_extract(ex, "out0", &score_8);
    ncnn_extractor_extract(ex, "out1", &score_16);
    ncnn_extractor_extract(ex, "out2", &score_32);
    ncnn_extractor_extract(ex, "out3", &bbox_8);
    ncnn_extractor_extract(ex, "out4", &bbox_16);
    ncnn_extractor_extract(ex, "out5", &bbox_32);
    ncnn_extractor_extract(ex, "out6", &kps_8);
    ncnn_extractor_extract(ex, "out7", &kps_16);
    ncnn_extractor_extract(ex, "out8", &kps_32);

    const ncnn_mat_t scoreMats[] = {score_8, score_16, score_32};
    const ncnn_mat_t bboxMats[]  = {bbox_8, bbox_16, bbox_32};
    const ncnn_mat_t kpsMats[]   = {kps_8, kps_16, kps_32};

    // 2 anchors per location, strides 8/16/32
    // 640x640: 80x80x2=12800, 40x40x2=3200, 20x20x2=800

    const s32 strides[]      = {8, 16, 32};
    const s32 anchorCounts[] = {12800, 3200, 800};

    f32 score_threshold = 0.25;

    for (s32 s = 0; s < 3; s++)
    {
        s32 stride = strides[s];
        s32 count = anchorCounts[s];

        s32 cols = model_face.net_w / stride; // feature map width
        s32 rows = model_face.net_h / stride; // feature map height

        // anchor base size matches stride
        f32 anchor_half = stride * 0.5f;

        // flat pointers — layout is anchor-major [count x channels]
        // for dims=2: w=channels, h=count, data is row-major
        const f32* score = (const f32*)ncnn_mat_get_data(scoreMats[s]);
        const f32* bbox  = (const f32*)ncnn_mat_get_data(bboxMats[s]);
        const f32* kps   = (const f32*)ncnn_mat_get_data(kpsMats[s]);

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
                    if (prob < score_threshold)
                        continue;

                    // anchor center
                    f32 cx = j * stride;
                    f32 cy = i * stride;

                    f32 l = bbox[idx * 4 + 0] * stride;
                    f32 t = bbox[idx * 4 + 1] * stride;
                    f32 r = bbox[idx * 4 + 2] * stride;
                    f32 b = bbox[idx * 4 + 3] * stride;

                    f32 x1 = cx - l;
                    f32 y1 = cy - t;
                    f32 x2 = cx + r;
                    f32 y2 = cy + b;

                    s32 x1s = (s32)((x1 - image->pad_x) / image->scale);
                    s32 y1s = (s32)((y1 - image->pad_y) / image->scale);
                    s32 x2s = (s32)((x2 - image->pad_x) / image->scale);
                    s32 y2s = (s32)((y2 - image->pad_y) / image->scale);

                    Box box;
                    box.x = (u16)(x1s);
                    box.y = (u16)(y1s);
                    box.w = (u16)(x2s - x1s);
                    box.h = (u16)(y2s - y1s);

                    u32 image_src_w = (u32)(image->w / image->scale);
                    u32 image_src_h = (u32)(image->h / image->scale);

                    box.x = CLAMP(box.x, 0, image_src_w - 1);
                    box.y = CLAMP(box.y, 0, image_src_h - 1);
                    box.w = CLAMP(box.w, 1, image_src_w - box.x - 1);
                    box.h = CLAMP(box.h, 1, image_src_h - box.y - 1);

                    box.confidence = (u16)(prob*100);

                    // landmarks: 5 keypoints, (dx,dy) relative to anchor center
                    for (s32 k = 0; k < 5; k++)
                    {
                        f32 lx = cx + kps[idx*10 + k*2 + 0] * stride;
                        f32 ly = cy + kps[idx*10 + k*2 + 1] * stride;
                        box.landmarks[k].x = (u16)((lx - image->pad_x) / image->scale);
                        box.landmarks[k].y = (u16)((ly - image->pad_y) / image->scale);
                    }

                    list_add(&boxes, &box);
                }
            }
        }
    }

    // cleanup
    ncnn_mat_destroy(score_8);
    ncnn_mat_destroy(score_16);
    ncnn_mat_destroy(score_32);
    ncnn_mat_destroy(bbox_8);
    ncnn_mat_destroy(bbox_16);
    ncnn_mat_destroy(bbox_32);
    ncnn_mat_destroy(kps_8);
    ncnn_mat_destroy(kps_16);
    ncnn_mat_destroy(kps_32);

    ncnn_extractor_destroy(ex);
    ncnn_mat_destroy(input);

    logv("Found %u boxes before nms", boxes.count);

    boxes = non_maximum_suppression(boxes, 0.45);

    return boxes;
}

List detect_persons(Arena *arena, Image *image)
{
    List boxes = list_create(arena, sizeof(Box));

    Box box1 = {50, 50, 100, 50};
    list_add(&boxes, &box1);

    return boxes;
}

void box_print(Box *b)
{
    logv("Box: [ %u %u %u %u ], Confidence: %u", b->x, b->y, b->w, b->h, b->confidence);
}

String detect_type_to_string(DetectType type)
{
    switch(type)
    {
        case DETECT_TYPE_FACE:          return S("face");
        case DETECT_TYPE_PERSON:        return S("person");
        case DETECT_TYPE_LICENSE_PLATE: return S("license_plate");
        case DETECT_TYPE_DOCUMENT:      return S("document");
        case DETECT_TYPE_NONE:
        default:
    }

    return S("none");
}

DetectType detect_type_from_string(String str)
{
    if(string_equal(str, S("face")))
        return DETECT_TYPE_FACE;

    if(string_equal(str, S("person")))
        return DETECT_TYPE_PERSON;

    if(string_equal(str, S("license_plate")))
        return DETECT_TYPE_LICENSE_PLATE;

    if(string_equal(str, S("document")))
        return DETECT_TYPE_DOCUMENT;

    return DETECT_TYPE_NONE;
}
