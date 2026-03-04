
static Model model_face    = {0};
static Model model_persons = {0};

b32 detect_init(void)
{
    if(!model_face.initialized)
    {
        model_face.net = ncnn_net_create();
        model_face.net_w = 640;
        model_face.net_h = 640;
        ncnn_net_load_param(model_face.net, "models/scrfd_500m_gnkps.ncnn.param");
        ncnn_net_load_model(model_face.net, "models/scrfd_500m_gnkps.ncnn.bin");
    }

    if(!model_persons.initialized)
    {
        model_persons.net = ncnn_net_create();
        model_persons.net_w = 640;
        model_persons.net_h = 640;
        ncnn_net_load_param(model_persons.net, "models/scrfd_person_2.5g.ncnn.param");
        ncnn_net_load_model(model_persons.net, "models/scrfd_person_2.5g.ncnn.bin");
    }

    return true;
}

void *detect(void *args)
{
    DetectArgs *detect_args = (DetectArgs *)args;

    Image *image       = detect_args->image;
    List  *total_boxes = detect_args->boxes;

    DetectType type = detect_args->type;

    List new_boxes  = {0};

    ArenaTemp scratch = scratch_begin();

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

        for(u64 j = i + 1; i < boxes_arr.count; ++j)
        {
            if(suppressed[j]) continue;

            Box *box_b = (Box *)(boxes_arr.items + j*sizeof(Box));

            u32 bx1 = box_b->x;
            u32 by1 = box_b->y;
            u32 bx2 = box_b->x + box_b->w;
            u32 by2 = box_b->y + box_b->h;

            f64 b_area = box_b->w * box_b->h;

            u32 ix1 = MAX(ax1, bx1);
            u32 iy1 = MAX(ay1, by1);
            u32 ix2 = MIN(ax2, bx2);
            u32 iy2 = MIN(ay2, by2);

            u64 inter = MAX(0, ix2 - ix1) * MAX(0, iy2 - iy1);

            f32 iou = (f32)inter / (a_area + b_area - inter);

            if (iou > iou_threshold)
                suppressed[j] = true;
        }
    }

    return boxes_curated;
}

List detect_faces(Arena *arena, Image *image)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->w, image->h, image->n, 0);
    
    // Maps [0,255] --> [-1,1]

    const f32 mean[] = {127.5, 127.5, 127.5};
    const f32 norm[] = {1.0/128.0, 1.0/128.0, 1.0/128.0};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_extractor_t ex = ncnn_extractor_create(model_face.net);
    // ex.set_light_mode(true);

    ncnn_extractor_input(ex, "in0", input);

    ncnn_mat_t score_8, score_16, score_32;
    ncnn_mat_t bbox_8, bbox_16, bbox_32;
    ncnn_mat_t kps_8, kps_16, kps_32;

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

    const s32 strides[] = {8, 16, 32};
    const s32 anchorCounts[] = {12800, 3200, 800};

    f32 score_threshold = 0.25; // TODO

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
        const f32* score = (const f32*)scoreMats[s]; // [count x 1]
        const f32* bbox  = (const f32*)bboxMats[s]; // [count x 4]
        const f32* kps   = (const f32*)kpsMats[s]; // [count x 10]

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
                    box.x = (s32)(((x1) - image->pad_x) / image->scale);
                    box.y = (s32)(((y1) - image->pad_y) / image->scale);
                    box.w = (s32)((x2 - x1) / image->scale);
                    box.h = (s32)((y2 - y1) / image->scale);

                    // this is maybe redundant
                    box.x = MAX(0, MIN(box.x, image->w - 1));
                    box.y = MAX(0, MIN(box.y, image->h - 1));
                    box.w = MAX(1, MIN(box.w, image->w - box.x));
                    box.h = MAX(1, MIN(box.h, image->h - box.y));

                    box.confidence = (s32)(prob * 100);

                    // landmarks: 5 keypoints, (dx,dy) relative to anchor center
                    for (s32 k = 0; k < 5; k++)
                    {
                        f32 lx = cx + kps[idx * 10 + k * 2 + 0] * stride;
                        f32 ly = cy + kps[idx * 10 + k * 2 + 1] * stride;
                        box.landmarks[k].x = (s32)((lx - image->pad_x) / image->scale);
                        box.landmarks[k].y = (s32)((ly - image->pad_y) / image->scale);
                    }

                    list_add(&boxes, &box);
                }
            }
        }
    }

    boxes = non_maximum_suppression(&boxes, 0.45);

    return boxes;
}

List detect_persons(Arena *arena, Image *image)
{
    List boxes = list_create(arena, sizeof(Box));

    Box box1 = {50, 50, 100, 50};
    list_add(&boxes, &box1);

    return boxes;
}
