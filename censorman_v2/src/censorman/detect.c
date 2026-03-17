static Model model_face   = {0};
static Model model_person = {0};

extern void ncnn_net_set_lightmode(ncnn_net_t net, int enable);
extern void ncnn_extractor_clear(ncnn_extractor_t ex);
extern void ncnn_net_set_workspace_allocator(ncnn_net_t net);

static Model model_create(Arena *arena, s32 thread_count, const char *path_param, const char *path_bin)
{
    Model model = {0};

    model.net_w = 640;
    model.net_h = 640;

    model.nets = PUSH_ARRAY(arena, ncnn_net_t, thread_count);
    for(s64 i = 0; i < thread_count; ++i)
    {
        model.nets[i] = ncnn_net_create();

        //ncnn_net_set_workspace_allocator(model.nets[i]);
        ncnn_net_set_lightmode(model.nets[i], 1);
        ncnn_option_t opt = ncnn_net_get_option(model.nets[i]);
        ncnn_option_set_num_threads(opt, 1); // @LOOKAT
        ncnn_option_set_use_packing_layout(opt, 1); // faster SIMD on x86
        ncnn_net_set_option(model.nets[i], opt);

        int param_ret = ncnn_net_load_param(model.nets[i], path_param);
        int model_ret = ncnn_net_load_model(model.nets[i], path_bin);

        model.initialized |= (param_ret == 0 && model_ret == 0);
    }

    return model;
}

b32 detect_init(Arena *arena, s32 thread_count)
{
    model_face = model_create(arena, thread_count,
            "models/scrfd_500m_gnkps.ncnn.param",
            "models/scrfd_500m_gnkps.ncnn.bin"
    );

    model_person = model_create(arena, thread_count,
            "models/scrfd_person_2.5g.ncnn.param",
            "models/scrfd_person_2.5g.ncnn.bin"
    );

    if(!model_face.initialized || !model_person.initialized)
    {
        loge("Failed with model loading. Loaded [ Face: %s, Person: %s ]", STR_BOOL(model_face.initialized), STR_BOOL(model_person.initialized));
        return false;
    }

    return true;
}

void detect(void *args)
{
    DetectArgs *detect_args = (DetectArgs *)args;

    Image *image       = detect_args->image;
    List  *total_boxes = detect_args->boxes;
    s64   thread_index = detect_args->thread_index;

    stopwatch_begin(image->stopwatch, S("detect"));

    DetectType type = detect_args->type;

    List new_boxes  = {0};

    Temp scratch = scratch_begin();

    switch(type)
    {
        case DETECT_TYPE_FACE:
        {
            new_boxes = detect_faces(scratch.arena, image, thread_index);
        } break;
        case DETECT_TYPE_PERSON:
        {
            new_boxes = detect_persons(scratch.arena, image, thread_index);
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
}

BoxFrame convert_list_to_box_frame(Arena *arena, List box_list, u32 frame_number)
{
    BoxFrame bf = {0};

    if(!arena)
        return bf;

    bf.boxes        = PUSH_ARRAY(arena, Box, box_list.count);
    bf.box_count    = box_list.count;
    bf.frame_number = frame_number;

    ListNode *ln = box_list.head;
    if(!ln) return bf;

    u32 box_counter = 0;
    for(;;)
    {
        if(box_counter >= box_list.count)
            break;

        Box *box = (Box *)ln->item;
        if(box) MemoryCopy(&bf.boxes[box_counter], box, sizeof(Box));
        box_counter++;

        if(!ln->next)
            break;

        ln = ln->next;
    }

    return bf;
}

void detect_interpolate_boxes(Video *vid, BoxFrame *box_frames)
{
    // Interpolate detection boxes for frames that are
    // missing boxes
    //  
    //               Frames
    //
    //    --|--------|--------|--------|--...
    //     f0        x        x        f1
    //     /        /        /         /
    //   [filled]  [gap]    [gap]    [filled]
    //


    if(!vid || !box_frames)
        return;

    BoxFrame *f0 = NULL;
    BoxFrame *f1 = NULL;

    for(u32 i = 0; i < vid->frame_count; )
    {
        BoxFrame *curr = &box_frames[i];

        // set previous frame
        f0 = f1;

        if(curr->box_count > 0)
        {
            // filled
            f1 = curr;
            i++;
            continue;
        }

        // first gap frame

        // find next filled frame
        u32 j = i+1;
        BoxFrame *box_frame_j = NULL;

        for(;;)
        {
            if(j >= vid->frame_count) break;
            box_frame_j = &box_frames[j];
            if(box_frame_j && box_frame_j->box_count > 0) break;
            j++;
        }

        s32 frames_in_between = j - i;

        if(j >= vid->frame_count)
        {
            // end of video

            if(f0)
            {
                // no future valid frame, copy f0 forward for remaining frames
                for(u32 f = 0; f < frames_in_between; ++f)
                {
                    BoxFrame *frame = &box_frames[i+f];
                    frame->boxes = PUSH_ARRAY(vid->arena, Box, frame->box_count);
                    frame->box_count = f0->box_count;
                    frame->frame_number = i+f;
                    MemoryCopy(frame->boxes, f0->boxes, f0->box_count*sizeof(Box));
                }
            }
            break;
        }

        // set f1 to next filled frame
        f1 = box_frame_j;

        // interpolate frames between f0 and f1
        for(u32 f = 0; f < frames_in_between; ++f)
        {
            BoxFrame *frame = &box_frames[i+f];

            if(!f0)
            {
                // no previous frame, copy f1 backward
                frame->box_count = f1->box_count;
                frame->boxes = PUSH_ARRAY(vid->arena, Box, frame->box_count);
                frame->frame_number = i+f;
                frame->interpolated = true;
                MemoryCopy(frame->boxes, f1->boxes, f1->box_count * sizeof(Box));
                continue;
            }

            // Decide which box frame has more boxes
            BoxFrame *a = (f0->box_count >= f1->box_count) ? f0 : f1;
            BoxFrame *b = (f0->box_count >= f1->box_count) ? f1 : f0;

            frame->box_count = a->box_count;
            frame->boxes = PUSH_ARRAY(vid->arena, Box, frame->box_count);
            frame->frame_number = i+f;
            frame->interpolated = true;

            // Match boxes and smooth position exponentially

            u32 matched_count = 0;
            u32 *matches = PUSH_ARRAY(vid->arena, u32, a->box_count);

            for(u32 k = 0; k < a->box_count; ++k)
            {
                f32 min_mv = 3.4e38;
                s32 min_index = -1;

                for(u32 l = 0; l < b->box_count; ++l)
                {
                    b32 already_matched = false;
                    for(u32 m = 0; m < matched_count; ++m)
                    {
                        if(matches[m] == l)
                        {
                            already_matched = true;
                            break;
                        }
                    }

                    if(already_matched) continue;

                    Box *ra = &a->boxes[k];
                    Box *rb = &b->boxes[l];

                    // find center point of boxes

                    f32 acx = ra->x + ra->w * 0.5;
                    f32 acy = ra->y + ra->h * 0.5;

                    f32 bcx = rb->x + rb->w * 0.5;
                    f32 bcy = rb->y + rb->h * 0.5;

                    f32 mv = ABS(acx - bcx) + ABS(acy - bcy);

                    if(mv < min_mv)
                    {
                        min_mv = mv;
                        min_index = l;
                    }
                }

                Box *box_interpolated = &frame->boxes[k];

                if(min_index >= 0)
                {
                    // mark as matched
                    matches[matched_count++] = min_index;
                    
                    Box *ra = &a->boxes[k];
                    Box *rb = &b->boxes[min_index];

                    const f32 alpha = 0.4; // smoothing

                    box_interpolated->x = (s32)interp_exp_smooth((f32)ra->x, (f32)rb->x, alpha, f);
                    box_interpolated->y = (s32)interp_exp_smooth((f32)ra->y, (f32)rb->y, alpha, f);
                    box_interpolated->w = (s32)interp_exp_smooth((f32)ra->w, (f32)rb->w, alpha, f);
                    box_interpolated->h = (s32)interp_exp_smooth((f32)ra->h, (f32)rb->h, alpha, f);
                    box_interpolated->confidence = (s32)interp_exp_smooth((f32)ra->confidence, (f32)rb->confidence, alpha, f);
                    box_interpolated->type = ra->type;

                    for(s32 j2 = 0; j2 < 5; ++j2)
                    {
                        box_interpolated->landmarks[j2].x = (s32)interp_exp_smooth((f32)ra->landmarks[j2].x, (f32)rb->landmarks[j2].x, alpha, f);
                        box_interpolated->landmarks[j2].y = (s32)interp_exp_smooth((f32)ra->landmarks[j2].y, (f32)rb->landmarks[j2].y, alpha, f);
                    }
                }
                else
                {
                    // No match, just copy forward from a
                    MemoryCopy(box_interpolated, &a->boxes[k], sizeof(Box));
                }
            }
        }

        // Advance i past interpolated frames
        i += frames_in_between;
    }
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

        Box *box_a = (Box *)(((u8 *)boxes_arr.items) + i*sizeof(Box));
        list_add(&boxes_curated, box_a);

        u32 ax1 = box_a->x;
        u32 ay1 = box_a->y;
        u32 ax2 = box_a->x + box_a->w;
        u32 ay2 = box_a->y + box_a->h;

        f64 a_area = box_a->w * box_a->h;

        for(u64 j = i + 1; j < boxes_arr.count; ++j)
        {
            if(suppressed[j]) continue;

            Box *box_b = (Box *)(((u8 *)boxes_arr.items) + j*sizeof(Box));

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

    return boxes_curated;
}

List detect_faces(Arena *arena, Image *image, s64 thread_index)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->props.w, image->props.h, image->props.w*3, 0);
    
    // Maps [0,255] --> [-1,1]

    const f32 mean[] = {127.5, 127.5, 127.5};
    const f32 norm[] = {1.0/128.0, 1.0/128.0, 1.0/128.0};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_net_t net = model_face.nets[thread_index];
    ncnn_extractor_t ex = ncnn_extractor_create(net);

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

    f32 score_threshold = 0.32f;

    for(s32 s = 0; s < 3; ++s)
    {
        s32 stride = strides[s];

        s32 cols = model_face.net_w / stride; // feature map width
        s32 rows = model_face.net_h / stride; // feature map height

        // flat pointers — layout is anchor-major [count x channels]
        // for dims=2: w=channels, h=count, data is row-major
        const f32* score = (const f32*)ncnn_mat_get_data(scoreMats[s]);
        const f32* bbox  = (const f32*)ncnn_mat_get_data(bboxMats[s]);
        const f32* kps   = (const f32*)ncnn_mat_get_data(kpsMats[s]);

        for(s32 a = 0; a < 2; ++a)
        {
            for(s32 i = 0; i < rows; ++i)
            {
                for(s32 j = 0; j < cols; ++j)
                {
                    // interleaved anchor index:
                    // anchor 0 for all locations, then anchor 1
                    s32 idx = (i*cols+j)*2+a;

                    f32 prob = score ? score[idx] : 0.0f;
                    if (prob < score_threshold)
                        continue;

                    // anchor center
                    f32 cx = j * stride;
                    f32 cy = i * stride;

                    f32 x1 = cx - bbox[idx*4+0] * stride;
                    f32 y1 = cy - bbox[idx*4+1] * stride;
                    f32 x2 = cx + bbox[idx*4+2] * stride;
                    f32 y2 = cy + bbox[idx*4+3] * stride;

                    Box box = {0};
                    box.x = (s32)x1;
                    box.y = (s32)y1;
                    box.w = (s32)(x2 - x1);
                    box.h = (s32)(y2 - y1);
                    box.confidence = (u16)(prob * 100);
                    box.type = DETECT_TYPE_FACE;

                    for(s32 k = 0; k < LANDMARK_COUNT; k++)
                    {
                        box.landmarks[k].x = (s32)(cx + kps[idx*10 + k*2 + 0] * stride);
                        box.landmarks[k].y = (s32)(cy + kps[idx*10 + k*2 + 1] * stride);
                    }

                    box = box_unscale(box, image);
                    box = box_rotate(box, image, image->props_orig.rotation, CW);
#if 0
                    logw("Box [%d], confidence: %u", boxes.count, box.confidence);
                    logw("  after detect: [%-4d %-4d %-4d %-4d]", box.x, box.y, box.w, box.h);
                    logw("  after unscale: [%-4d %-4d %-4d %-4d]", box.x, box.y, box.w, box.h);
                    logw("  after rotate: [%-4d %-4d %-4d %-4d]", box.x, box.y, box.w, box.h);
#endif

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

    boxes = non_maximum_suppression(boxes, 0.45);

    if(os_get_log_level() == LOG_LEVEL_VERBOSE)
    {
        logv("Found %d boxes", boxes.count);
        for(s64 i = 0; i < boxes.count; ++i)
        {
            Box *b = (Box *)list_get(&boxes, i);
            box_print(b);
        }
    }

    return boxes;
}

List detect_persons(Arena *arena, Image *image, s64 thread_index)
{
    List boxes = list_create(arena, sizeof(Box));

    Box box1 = {50, 50, 100, 50};
    list_add(&boxes, &box1);

    return boxes;
}

Box box_unscale(Box box, Image *image)
{
    const f32 scale_factor = image->props.scale == 0.0 ? 1.0 : 1.0 / image->props.scale;

    b32 swap_dimensions = (image->props_orig.rotation == ROTATE_90 || image->props_orig.rotation == ROTATE_270);

    s32 src_w = swap_dimensions ? image->props_orig.h : image->props_orig.w;
    s32 src_h = swap_dimensions ? image->props_orig.w : image->props_orig.h;

    box.x = (s32)((box.x - image->props.pad_x) * scale_factor);
    box.y = (s32)((box.y - image->props.pad_y) * scale_factor);
    box.w = (s32)(box.w * scale_factor);
    box.h = (s32)(box.h * scale_factor);

    box.x = CLAMP(box.x, 0, src_w - 1);
    box.y = CLAMP(box.y, 0, src_h - 1);
    box.w = CLAMP(box.w, 1, src_w - box.x - 1);
    box.h = CLAMP(box.h, 1, src_h - box.y - 1);

    for(s32 k = 0; k < LANDMARK_COUNT; k++)
    {
        Point *lm = &box.landmarks[k];

        lm->x = (s32)((lm->x - image->props.pad_x) * scale_factor);
        lm->y = (s32)((lm->y - image->props.pad_y) * scale_factor);

        lm->x = CLAMP(lm->x, 0, src_w - 1);
        lm->y = CLAMP(lm->y, 0, src_h - 1);
    }

    return box;
}

Box box_rotate(Box box, Image *image, Rotation rotation, ClockDir dir)
{
    if(rotation == ROTATE_0) return box;

    b32 swap_dimensions = (image->props_orig.rotation == ROTATE_90 || image->props_orig.rotation == ROTATE_270);

    s32 src_w = swap_dimensions ? image->props_orig.h : image->props_orig.w;
    s32 src_h = swap_dimensions ? image->props_orig.w : image->props_orig.h;

    // normalize to CW
    Rotation effective = rotation;
    if(dir == CCW)
    {
        switch(rotation)
        {
            case ROTATE_90:  effective = ROTATE_270; break;
            case ROTATE_270: effective = ROTATE_90;  break;
            default: break;
        }
    }

    f32 cx = box.x + box.w * 0.5f;
    f32 cy = box.y + box.h * 0.5f;
    f32 rcx, rcy;

    switch(effective)
    {
        case ROTATE_90:
            rcx = src_h - cy;
            rcy = cx;
            SWAP(s32, box.w, box.h);
            break;
        case ROTATE_180:
            rcx = src_w - cx;
            rcy = src_h - cy;
            break;
        case ROTATE_270:
            rcx = cy;
            rcy = src_w - cx;
            SWAP(s32, box.w, box.h);
            break;
        default:
            return box;
    }

    box.x = (s32)(rcx - box.w * 0.5);
    box.y = (s32)(rcy - box.h * 0.5);

    for(s32 k = 0; k < LANDMARK_COUNT; k++)
    {
        f32 lx = box.landmarks[k].x;
        f32 ly = box.landmarks[k].y;

        switch(effective)
        {
            case ROTATE_90:
                box.landmarks[k].x = (s32)(src_h - ly);
                box.landmarks[k].y = (s32)(lx);
                break;
            case ROTATE_180:
                box.landmarks[k].x = (s32)(src_w - lx);
                box.landmarks[k].y = (s32)(src_h - ly);
                break;
            case ROTATE_270:
                box.landmarks[k].x = (s32)(ly);
                box.landmarks[k].y = (s32)(src_w - lx);
                break;
            default: break;
        }
    }

    return box;
}

void box_print(Box *b)
{
    if(!b) return;

    logv("Box: (" STR_FMT ") [%-4d %-4d %-4d %-4d], Confidence: %2u, Landmarks: [(%-4d,%-4d),(%-4d,%-4d),(%-4d,%-4d),(%-4d,%-4d),(%-4d,%-4d)]",
            STR_ARG(detect_type_to_string(b->type)),
            b->x, b->y, b->w, b->h, b->confidence,
            b->landmarks[0].x, b->landmarks[0].y,
            b->landmarks[1].x, b->landmarks[1].y,
            b->landmarks[2].x, b->landmarks[2].y,
            b->landmarks[3].x, b->landmarks[3].y,
            b->landmarks[4].x, b->landmarks[4].y
        );
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

