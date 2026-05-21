
#include "model_data/scrfd_face_2.5g_bnkps_bin.h"
#include "model_data/scrfd_face_10g_bnkps_bin.h"
#include "model_data/scrfd_person_bin.h"
#include "model_data/license_plate_bin.h"
#include "model_data/nudity_bin.h"

static Model model_face      = {0};
static Model model_person = {0};
static Model model_license_plate = {0};
static Model model_nudity = {0};

static u32 scrfd_face_in0  = SCRFD_FACE_IN0;
static u32 scrfd_face_out0 = SCRFD_FACE_OUT0;
static u32 scrfd_face_out1 = SCRFD_FACE_OUT1;
static u32 scrfd_face_out2 = SCRFD_FACE_OUT2;
static u32 scrfd_face_out3 = SCRFD_FACE_OUT3;
static u32 scrfd_face_out4 = SCRFD_FACE_OUT4;
static u32 scrfd_face_out5 = SCRFD_FACE_OUT5;
static u32 scrfd_face_out6 = SCRFD_FACE_OUT6;
static u32 scrfd_face_out7 = SCRFD_FACE_OUT7;
static u32 scrfd_face_out8 = SCRFD_FACE_OUT8;

extern void ncnn_net_set_lightmode(ncnn_net_t net, int enable);
extern void ncnn_extractor_clear(ncnn_extractor_t ex);
extern void ncnn_net_set_workspace_allocator(ncnn_net_t net);

static b32  _validate_facial_geometry(Box *box);
static List _filter_on_facial_geometry(List boxes);

static Model model_create_mem(s32 net_w, s32 net_h, const u8 *param_bin, const u8 *model_bin)
{
    Model model = {0};

    model.net_w = net_w;
    model.net_h = net_h;

    s64 thread_count = s_thread_context.count;

    for(s64 i = 0; i < thread_count; ++i)
    {
        model.net = ncnn_net_create();

        // ncnn_net_set_workspace_allocator(model.net);
        ncnn_net_set_lightmode(model.net, 1);

        ncnn_option_t opt = ncnn_net_get_option(model.net);
        ncnn_option_set_num_threads(opt, 1);
        ncnn_option_set_use_packing_layout(opt, 1); // faster SIMD on x86
        ncnn_option_set_use_fp16_packed(opt, 1);
        ncnn_option_set_use_fp16_storage(opt, 1);
        ncnn_option_set_use_fp16_arithmetic(opt, 1);
        ncnn_net_set_option(model.net, opt);

        size_t param_ret = ncnn_net_load_param_bin_memory(model.net, param_bin);
        size_t model_ret = ncnn_net_load_model_memory(model.net, model_bin);

        model.initialized = (param_ret > 0 && model_ret > 0);
    }

    return model;
}

b32 detect_init(DetectConfig *detect_cfgs, s64 config_count)
{
    for(s64 i = 0; i < config_count; ++i)
    {
        DetectType type = detect_cfgs[i].type;

        switch(type)
        {
            case DETECT_TYPE_FACE:
            {
                model_face = model_create_mem(640, 640,
                        scrfd_2_5g_bnkps_opt_ncnn_param_bin,
                        scrfd_2_5g_bnkps_opt_ncnn_bin
                );

                scrfd_face_in0  = SCRFD_FACE_IN0;
                scrfd_face_out0 = SCRFD_FACE_OUT0;
                scrfd_face_out1 = SCRFD_FACE_OUT1;
                scrfd_face_out2 = SCRFD_FACE_OUT2;
                scrfd_face_out3 = SCRFD_FACE_OUT3;
                scrfd_face_out4 = SCRFD_FACE_OUT4;
                scrfd_face_out5 = SCRFD_FACE_OUT5;
                scrfd_face_out6 = SCRFD_FACE_OUT6;
                scrfd_face_out7 = SCRFD_FACE_OUT7;
                scrfd_face_out8 = SCRFD_FACE_OUT8;

            } break;
            case DETECT_TYPE_FACE_10G:
            {
                model_face = model_create_mem(640, 640,
                        scrfd_10g_bnkps_opt_param_bin,
                        scrfd_10g_bnkps_opt_bin
                );

                scrfd_face_in0  = SCRFD_FACE_10G_IN0;
                scrfd_face_out0 = SCRFD_FACE_10G_OUT0;
                scrfd_face_out1 = SCRFD_FACE_10G_OUT1;
                scrfd_face_out2 = SCRFD_FACE_10G_OUT2;
                scrfd_face_out3 = SCRFD_FACE_10G_OUT3;
                scrfd_face_out4 = SCRFD_FACE_10G_OUT4;
                scrfd_face_out5 = SCRFD_FACE_10G_OUT5;
                scrfd_face_out6 = SCRFD_FACE_10G_OUT6;
                scrfd_face_out7 = SCRFD_FACE_10G_OUT7;
                scrfd_face_out8 = SCRFD_FACE_10G_OUT8;
            } break;
            case DETECT_TYPE_PERSON:
            {
                if(!model_person.initialized)
                {
                    model_person = model_create_mem(640, 640,
                            scrfd_person_2_5g_ncnn_param_bin,
                            scrfd_person_2_5g_ncnn_bin
                    );
                }
            } break;
            case DETECT_TYPE_LICENSE_PLATE:
            {
                if(!model_license_plate.initialized)
                {
                    model_license_plate = model_create_mem(640, 640,
                            license_plate_ncnn_param_bin,
                            license_plate_ncnn_bin
                    );
                }
            } break;
            case DETECT_TYPE_NUDITY:
            {
                if(!model_nudity.initialized)
                {
                    model_nudity = model_create_mem(320, 320,
                        nudity_ncnn_param_bin,
                        nudity_ncnn_bin
                    );
                }
            } break;
            default:
            break;
        }
    }

    return true;
}

void detect(DetectConfig *cfg, Image *image, List *total_boxes)
{
    stopwatch_begin(image->stopwatch, S("detect"));

    f32 threshold_confidence = cfg->threshold_confidence;
    f32 threshold_nms        = cfg->threshold_nms;

    List new_boxes  = {0};

    Temp scratch = scratch_begin();

    switch(cfg->type)
    {
        case DETECT_TYPE_FACE:
        case DETECT_TYPE_FACE_10G:
        {
            new_boxes = detect_faces(scratch.arena, image, threshold_confidence, threshold_nms);
        } break;
        case DETECT_TYPE_PERSON:
        {
            new_boxes = detect_persons(scratch.arena, image, threshold_confidence, threshold_nms);
        } break;
        case DETECT_TYPE_LICENSE_PLATE:
        {
            new_boxes = detect_license_plates(scratch.arena, image, threshold_confidence, threshold_nms);
        } break;
        case DETECT_TYPE_NUDITY:
        {
            new_boxes = detect_nudity(scratch.arena, image, threshold_confidence, threshold_nms);
        } break;
        default:
            logw("Unknown detect kind: %d", cfg->type);
            break;
    }

    if(os_get_log_level() == LOG_LEVEL_VERBOSE)
    {
        if(new_boxes.count > 0)
            logv("Found %d boxes", new_boxes.count);

        for(s64 i = 0; i < new_boxes.count; ++i)
        {
            Box *box = (Box *)list_get(&new_boxes, i);
            box_print(box, s_log_level);
        }
    }

    // add new boxes to total list
    list_add_list(total_boxes, &new_boxes);

    scratch_end(scratch);

    stopwatch_end(image->stopwatch, S("detect"));
}

BoxFrame box_frame_from_list(Arena *arena, List box_list, u32 frame_number)
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

BoxFrame box_frame_divide_into_features(Arena *arena, BoxFrame input, ImageProps *props, u8 facial_features)
{
    BoxFrame output = input;

    if(facial_features == FACIAL_FEATURE_NONE)
        return output;

    b8 feature_eyes     = BIT_CHECK(facial_features, FACIAL_FEATURE_EYES);
    b8 feature_nose     = BIT_CHECK(facial_features, FACIAL_FEATURE_NOSE);
    b8 feature_mouth    = BIT_CHECK(facial_features, FACIAL_FEATURE_MOUTH);
    b8 feature_cheeks   = BIT_CHECK(facial_features, FACIAL_FEATURE_CHEEKS);
    b8 feature_forehead = BIT_CHECK(facial_features, FACIAL_FEATURE_FOREHEAD);

    // pre-gather how many face boxes there are
    // and allocate the new number of facial boxes

    s64 box_count_new = 0;
    for(s64 i = 0; i < input.box_count; ++i)
    {
        Box *box = &input.boxes[i];
        if(box->type == DETECT_TYPE_FACE || box->type == DETECT_TYPE_FACE_10G)
        {
            box_count_new += (2*feature_eyes); // there are two eyes <o>_<o>
            box_count_new += feature_nose;
            box_count_new += feature_mouth;
            box_count_new += (2*feature_cheeks); // two cheeks
            box_count_new += feature_forehead;
        }
        else
        {
            // not a face, just keep the same box
            box_count_new += 1;
        }
    }

    output.boxes = PUSH_ARRAY(arena, Box, box_count_new);
    output.box_count = 0;

    // Deconstruct a face box into 5 small boxes for each facial feature
    // for all face boxes in a box frame

    for(s64 i = 0; i < input.box_count; ++i)
    {
        //Box input_box = input.boxes[i];
        Box input_box = input.boxes[i];

        b32 sideways = (props->rotation == ROTATE_90 || props->rotation == ROTATE_270);

        if(sideways)
        {
            SWAP(s32, input_box.w, input_box.h);
        }

        if(input_box.type != DETECT_TYPE_FACE && input_box.type != DETECT_TYPE_FACE_10G)
        {
            output.boxes[output.box_count++] = input_box;
            continue;
        }

        Point eye_left    = input_box.landmarks[0];
        Point eye_right   = input_box.landmarks[1];
        Point nose        = input_box.landmarks[2];
        Point mouth_left  = input_box.landmarks[3];
        Point mouth_right = input_box.landmarks[4];

        if(feature_eyes)
        {
            // Left eye

            Box box_eye_left  = {0};

            box_eye_left.w = 0.32f * input_box.w;
            box_eye_left.h = 0.18f * input_box.h;

            if(sideways)
            {
                SWAP(s32, box_eye_left.w, box_eye_left.h);
            }

            box_eye_left.x = eye_left.x - (0.50f*box_eye_left.w);
            box_eye_left.y = eye_left.y - (0.50f*box_eye_left.h);

            box_eye_left.confidence = input_box.confidence;
            box_eye_left.landmarks[0] = input_box.landmarks[0];
            box_eye_left.type = DETECT_TYPE_EYE;

            // Right eye

            Box box_eye_right = {0};

            box_eye_right.w = 0.32f * input_box.w;
            box_eye_right.h = 0.18f * input_box.h;

            if(sideways)
            {
                SWAP(s32, box_eye_right.w, box_eye_right.h);
            }

            box_eye_right.x = eye_right.x - (0.50f*box_eye_right.w);
            box_eye_right.y = eye_right.y - (0.50f*box_eye_right.h);

            box_eye_right.confidence = input_box.confidence;
            box_eye_right.landmarks[0] = input_box.landmarks[1];
            box_eye_right.type = DETECT_TYPE_EYE;

            if(sideways)
            {
                SWAP(s32, box_eye_right.w, box_eye_right.h);
            }

            output.boxes[output.box_count++] = box_eye_left;    
            output.boxes[output.box_count++] = box_eye_right;
        }

        if(feature_nose)
        {
            Box box_nose = {0};

            f32 nose_ratio_x = ABS(nose.x - input_box.x) / (f32)input_box.w;

            box_nose.w = 0.30f * input_box.w;
            box_nose.h = 0.20f * input_box.h;

            if(sideways)
            {
                SWAP(s32, box_nose.w, box_nose.h);
            }

            box_nose.x = nose.x - nose_ratio_x * box_nose.w;
            box_nose.y = nose.y - 0.75f * box_nose.h;

            box_nose.confidence = input_box.confidence;
            box_nose.landmarks[0] = input_box.landmarks[2];
            box_nose.type = DETECT_TYPE_NOSE;

            output.boxes[output.box_count++] = box_nose;
        }

        if(feature_mouth)
        {
            Box box_mouth = {0};

            s32 mouth_dist_x = ABS(mouth_right.x - mouth_left.x);
            s32 mouth_dist_y = ABS(mouth_right.y - mouth_left.y);

            Point mouth_middle =
            {
                .x = MIN(mouth_left.x, mouth_right.x) + 0.50f*mouth_dist_x,
                .y = MIN(mouth_left.y, mouth_right.y) + 0.50f*mouth_dist_y
            };

            if(sideways)
            {
                box_mouth.w = ABS(mouth_right.x - mouth_left.x) + 0.2f*input_box.h;
                box_mouth.h = ABS(mouth_right.y - mouth_left.y) + 0.2f*input_box.w;
                box_mouth.x = mouth_middle.x - 0.35f*box_mouth.w;
                box_mouth.y = mouth_middle.y - 0.50f*box_mouth.h;
            }
            else
            {
                box_mouth.w = ABS(mouth_right.x - mouth_left.x) + 0.2f*input_box.w;
                box_mouth.h = ABS(mouth_right.y - mouth_left.y) + 0.2f*input_box.h;
                box_mouth.x = mouth_middle.x - 0.50f*box_mouth.w;
                box_mouth.y = mouth_middle.y - 0.35f*box_mouth.h;
            }

            box_mouth.confidence = input_box.confidence;
            box_mouth.landmarks[0] = input_box.landmarks[3];
            box_mouth.landmarks[1] = input_box.landmarks[4];
            box_mouth.landmarks[2] = mouth_middle;
            box_mouth.type = DETECT_TYPE_MOUTH;

            output.boxes[output.box_count++] = box_mouth;
        }

        if(feature_cheeks)
        {
            Box box_cheek_left  = {0};
            Box box_cheek_right = {0};

            s32 eye_dist = (s32)vec2_distance(VEC2(eye_left.x, eye_left.y), VEC2(eye_right.x, eye_right.y));

            box_cheek_left.w = 0.32f * input_box.w;
            box_cheek_left.h = 0.20f * input_box.h;

            if(sideways)
            {
                box_cheek_left.x = (eye_left.x + 0.18f*input_box.h)  - (0.5f*box_cheek_left.h);
                box_cheek_left.y = (eye_left.y - 0.20f*eye_dist)     - (0.5f*box_cheek_left.w);
            }
            else
            {
                box_cheek_left.x = (eye_left.x - 0.20f*eye_dist)    - (0.5f*box_cheek_left.w);
                box_cheek_left.y = (eye_left.y + 0.18f*input_box.h) - (0.5f*box_cheek_left.h);
            }

            box_cheek_left.confidence = input_box.confidence;
            box_cheek_left.type = DETECT_TYPE_CHEEK;

            box_cheek_right.w = 0.35f * input_box.w;
            box_cheek_right.h = 0.20f * input_box.h;

            if(sideways)
            {
                box_cheek_right.x = (eye_right.x + 0.18f*input_box.h) - (0.5f*box_cheek_left.h);
                box_cheek_right.y = (eye_right.y - 0.20f*eye_dist)    - (0.5f*box_cheek_left.w);
            }
            else
            {
                box_cheek_right.x = (eye_right.x + 0.20f*eye_dist)    - (0.5f*box_cheek_right.w);
                box_cheek_right.y = (eye_right.y + 0.18f*input_box.h) - (0.5f*box_cheek_right.h);
            }


            box_cheek_right.confidence = input_box.confidence;
            box_cheek_right.type = DETECT_TYPE_CHEEK;

            output.boxes[output.box_count++] = box_cheek_left;    
            output.boxes[output.box_count++] = box_cheek_right;
        }

        if(feature_forehead)
        {
            Box box_forehead = {0};

            Point forehead_midpoint = {0};

            if(sideways)
            {
                forehead_midpoint.x = MIN(eye_left.x, eye_right.x) + ABS(eye_right.x - eye_left.x)*0.5f - (0.25*input_box.h);
                forehead_midpoint.y = MIN(eye_left.y, eye_right.y) + ABS(eye_right.y - eye_left.y)*0.5f;
            }
            else
            {
                forehead_midpoint.x = MIN(eye_left.x, eye_right.x) + ABS(eye_right.x - eye_left.x)*0.5f;
                forehead_midpoint.y = MIN(eye_left.y, eye_right.y) + ABS(eye_right.y - eye_left.y)*0.5f - (0.25*input_box.h);
            }

            box_forehead.w = 0.80f * input_box.w;
            box_forehead.h = 0.25f * input_box.h;

            if(sideways)
            {
                SWAP(s32, box_forehead.w, box_forehead.h);
            }

            box_forehead.x = forehead_midpoint.x - (0.5f * box_forehead.w);
            box_forehead.y = forehead_midpoint.y - (0.5f * box_forehead.h);

            box_forehead.confidence = input_box.confidence;
            box_forehead.type = DETECT_TYPE_FOREHEAD;
            box_forehead.landmarks[0] = forehead_midpoint;

            output.boxes[output.box_count++] = box_forehead;
        }
    }

    return output;
}

void box_frame_apply_padding(BoxFrame input, ImageProps *props, f32 padding_percent)
{
    for(s64 i = 0; i < input.box_count; ++i)
    {
        Box *box = &input.boxes[i];
        *box = box_pad(*box, props, padding_percent);
    }
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

        if(curr->detections_run)
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
            if(box_frame_j && box_frame_j->detections_run) break;
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
                    frame->frame_number = vid->frames_processed + i + f;
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
                frame->frame_number = vid->frames_processed + i + f;
                frame->interpolated = true;
                MemoryCopy(frame->boxes, f1->boxes, f1->box_count * sizeof(Box));
                continue;
            }

            // Decide which box frame has more boxes
            BoxFrame *a = (f0->box_count >= f1->box_count) ? f0 : f1;
            BoxFrame *b = (f0->box_count >= f1->box_count) ? f1 : f0;

            frame->box_count = a->box_count;
            frame->boxes = PUSH_ARRAY(vid->arena, Box, frame->box_count);
            frame->frame_number = vid->frames_processed + i + f;
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

                    if(ra->type != rb->type)
                        continue; // only match boxes of the same detect type

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
                    box_interpolated->confidence = (u8)interp_exp_smooth((f32)ra->confidence, (f32)rb->confidence, alpha, f);
                    box_interpolated->type = ra->type;

                    for(s32 j2 = 0; j2 < LANDMARK_COUNT; ++j2)
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

Model detect_get_model_by_type(DetectType type)
{
    switch(type)
    {
        case DETECT_TYPE_FACE:          return model_face;
        case DETECT_TYPE_FACE_10G:      return model_face;
        case DETECT_TYPE_PERSON:        return model_person;
        case DETECT_TYPE_LICENSE_PLATE: return model_license_plate;
        case DETECT_TYPE_NUDITY:        return model_nudity;
        case DETECT_TYPE_EYE:           return model_face;
        case DETECT_TYPE_NOSE:          return model_face;
        case DETECT_TYPE_MOUTH:         return model_face;
        case DETECT_TYPE_NONE:
        default: break;
    }

    Model model = {0};
    return model;
}

s32 box_compare(void *a, void *b)
{
    Box *box_a = (Box *)a;
    Box *box_b = (Box *)b;

    s32 result = (s32)(box_a->confidence < box_b->confidence) 
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

        s32 ax1 = box_a->x;
        s32 ay1 = box_a->y;
        s32 ax2 = box_a->x + box_a->w;
        s32 ay2 = box_a->y + box_a->h;

        f64 a_area = box_a->w * box_a->h;

        for(u64 j = i + 1; j < boxes_arr.count; ++j)
        {
            if(suppressed[j]) continue;

            Box *box_b = (Box *)(((u8 *)boxes_arr.items) + j*sizeof(Box));

            s32 bx1 = box_b->x;
            s32 by1 = box_b->y;
            s32 bx2 = box_b->x + box_b->w;
            s32 by2 = box_b->y + box_b->h;

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

// Anchor generation mirrors the reference retinaface C++ sample.
// base_size=16, ratio=1.0 for all strides.
// Scales per stride: stride32->[32,16], stride16->[8,4], stride8->[2,1]
// anchor_w = anchor_h = round(base_size / sqrt(ratio)) * scale
//                     = base_size * scale  (ratio==1 => sqrt==1)
static void retinaface_anchors(s32 base_size, f32 scale0, f32 scale1,
                                f32 out_anchors[2][4])
{
    // ratio == 1.0, so r_w = r_h = base_size
    f32 r = (f32)base_size;
    f32 cx = r * 0.5f;
    f32 cy = r * 0.5f;

    f32 scales[2] = {scale0, scale1};
    for(s32 a = 0; a < 2; ++a)
    {
        f32 rs = r * scales[a];
        out_anchors[a][0] = cx - rs * 0.5f; // x0
        out_anchors[a][1] = cy - rs * 0.5f; // y0
        out_anchors[a][2] = cx + rs * 0.5f; // x1
        out_anchors[a][3] = cy + rs * 0.5f; // y1
    }
}

List detect_faces(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->props.w, image->props.h, image->props.w*3, 0);

    // Maps [0,255] --> [-1,1]

    const f32 mean[] = {127.5f, 127.5f, 127.5f};
    const f32 norm[] = {1.0f/128.0f, 1.0f/128.0f, 1.0f/128.0f};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_net_t net = model_face.net;
    ncnn_extractor_t ex = ncnn_extractor_create(net);

    ncnn_extractor_input_index(ex, scrfd_face_in0, input);

    ncnn_mat_t score_8;
    ncnn_mat_t score_16;
    ncnn_mat_t score_32;
    ncnn_mat_t bbox_8;
    ncnn_mat_t bbox_16;
    ncnn_mat_t bbox_32;
    ncnn_mat_t kps_8;
    ncnn_mat_t kps_16;
    ncnn_mat_t kps_32;

    ncnn_extractor_extract_index(ex, scrfd_face_out0, &score_8);
    ncnn_extractor_extract_index(ex, scrfd_face_out1, &score_16);
    ncnn_extractor_extract_index(ex, scrfd_face_out2, &score_32);
    ncnn_extractor_extract_index(ex, scrfd_face_out3, &bbox_8);
    ncnn_extractor_extract_index(ex, scrfd_face_out4, &bbox_16);
    ncnn_extractor_extract_index(ex, scrfd_face_out5, &bbox_32);
    ncnn_extractor_extract_index(ex, scrfd_face_out6, &kps_8);
    ncnn_extractor_extract_index(ex, scrfd_face_out7, &kps_16);
    ncnn_extractor_extract_index(ex, scrfd_face_out8, &kps_32);

    const ncnn_mat_t scoreMats[] = {score_8, score_16, score_32};
    const ncnn_mat_t bboxMats[]  = {bbox_8, bbox_16, bbox_32};
    const ncnn_mat_t kpsMats[]   = {kps_8, kps_16, kps_32};

    // 2 anchors per location, strides 8/16/32
    // 640x640: 80x80x2=12800, 40x40x2=3200, 20x20x2=800

    const s32 strides[] = {8, 16, 32};

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
                    if (prob < threshold_confidence)
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
                    box.confidence = (u8)(prob * 100);
                    box.type = DETECT_TYPE_FACE;

                    for(s32 k = 0; k < LANDMARK_COUNT; k++)
                    {
                        box.landmarks[k].x = (s32)(cx + kps[idx*10 + k*2 + 0] * stride);
                        box.landmarks[k].y = (s32)(cy + kps[idx*10 + k*2 + 1] * stride);
                    }

                    box = box_unscale(box, image);
                    box = box_rotate(box, &image->props_orig, CW);

                    box_print(&box,LOG_LEVEL_VERBOSE);
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

    boxes = non_maximum_suppression(boxes, threshold_nms);

    return boxes;
}

List detect_persons(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->props.w, image->props.h, image->props.w*3, 0);

    const f32 mean[] = {127.5f, 127.5f, 127.5f};
    const f32 norm[] = {1.0f/128.0f, 1.0f/128.0f, 1.0f/128.0f};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_net_t net = model_person.net;
    ncnn_extractor_t ex = ncnn_extractor_create(net);

    ncnn_extractor_input_index(ex, SCRFD_PERSON_IN0, input);

    ncnn_mat_t score_8, score_16, score_32, score_64, score_128;
    ncnn_mat_t bbox_8,  bbox_16,  bbox_32,  bbox_64,  bbox_128;
    ncnn_mat_t kps_8,   kps_16,   kps_32,   kps_64,   kps_128;

    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT0, &score_8);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT1, &score_16);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT2, &score_32);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT3, &score_64);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT4, &score_128);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT5, &bbox_8);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT6, &bbox_16);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT7, &bbox_32);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT8, &bbox_64);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT9, &bbox_128);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT10, &kps_8);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT11, &kps_16);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT12, &kps_32);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT13, &kps_64);
    ncnn_extractor_extract_index(ex, SCRFD_PERSON_OUT14, &kps_128);

    const ncnn_mat_t scoreMats[] = {score_8,  score_16,  score_32,  score_64,  score_128};
    const ncnn_mat_t bboxMats[]  = {bbox_8,   bbox_16,   bbox_32,   bbox_64,   bbox_128};
    const ncnn_mat_t kpsMats[]   = {kps_8,    kps_16,    kps_32,    kps_64,    kps_128};

    const s32 strides[] = {8, 16, 32, 64, 128};

    for(s32 s = 0; s < 5; ++s)
    {
        s32 stride = strides[s];

        s32 cols = model_person.net_w / stride;
        s32 rows = model_person.net_h / stride;

        const f32 *score = (const f32 *)ncnn_mat_get_data(scoreMats[s]);
        const f32 *bbox  = (const f32 *)ncnn_mat_get_data(bboxMats[s]);
        const f32 *kps   = (const f32 *)ncnn_mat_get_data(kpsMats[s]);

        for(s32 i = 0; i < rows; ++i)
        {
            for(s32 j = 0; j < cols; ++j)
            {
                // 1 anchor per location
                s32 idx = i*cols + j;

                f32 prob = score ? score[idx] : 0.0f;
                if(prob < threshold_confidence)
                    continue;

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
                box.confidence = (u8)(prob * 100);
                box.type = DETECT_TYPE_PERSON;

                for(s32 k = 0; k < LANDMARK_COUNT; k++)
                {
                    box.landmarks[k].x = (s32)(cx + kps[idx*10 + k*2 + 0] * stride);
                    box.landmarks[k].y = (s32)(cy + kps[idx*10 + k*2 + 1] * stride);
                }

                box = box_unscale(box, image);
                box = box_rotate(box, &image->props_orig, CW);

                list_add(&boxes, &box);
            }
        }
    }

    // cleanup
    ncnn_mat_destroy(score_8);   ncnn_mat_destroy(score_16);  ncnn_mat_destroy(score_32);
    ncnn_mat_destroy(score_64);  ncnn_mat_destroy(score_128);
    ncnn_mat_destroy(bbox_8);    ncnn_mat_destroy(bbox_16);   ncnn_mat_destroy(bbox_32);
    ncnn_mat_destroy(bbox_64);   ncnn_mat_destroy(bbox_128);
    ncnn_mat_destroy(kps_8);     ncnn_mat_destroy(kps_16);    ncnn_mat_destroy(kps_32);
    ncnn_mat_destroy(kps_64);    ncnn_mat_destroy(kps_128);

    ncnn_extractor_destroy(ex);
    ncnn_mat_destroy(input);

    boxes = non_maximum_suppression(boxes, threshold_nms);
 
    return boxes;
}

List detect_license_plates(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->props.w, image->props.h, image->props.w*3, 0);

    const f32 mean[] = {0.0f, 0.0f, 0.0f};
    const f32 norm[] = {1.0f/255.0f, 1.0f/255.0f, 1.0f/255.0f};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_net_t net = model_license_plate.net;
    ncnn_extractor_t ex = ncnn_extractor_create(net);

    ncnn_extractor_input_index(ex, LICENSE_PLATE_IN0, input);

    ncnn_mat_t out0;
    ncnn_extractor_extract_index(ex, LICENSE_PLATE_OUT0, &out0);

    // out0 layout: [8400, 84] — cx, cy, w, h, score
    const f32 *data = (const f32 *)ncnn_mat_get_data(out0);

    s32 num_anchors  = ncnn_mat_get_w(out0); // 8400
    s32 num_channels = ncnn_mat_get_h(out0); // 84

    const f32 *cx_data = data + 0 * num_anchors;
    const f32 *cy_data = data + 1 * num_anchors;
    const f32 *w_data  = data + 2 * num_anchors;
    const f32 *h_data  = data + 3 * num_anchors;

    for(s32 i = 0; i < num_anchors; ++i)
    {
        // find max class score across all 80 classes
        f32 max_score = 0.0f;
        for(s32 c = 0; c < num_channels - 4; ++c)
        {
            f32 s = data[(4 + c) * num_anchors + i];
            if(s > max_score) max_score = s;
        }

        if(max_score < threshold_confidence)
            continue;

        f32 cx = cx_data[i];
        f32 cy = cy_data[i];
        f32 w  = w_data[i];
        f32 h  = h_data[i];
        
        Box box = {0};
        box.x = (s32)(cx - w * 0.5f);
        box.y = (s32)(cy - h * 0.5f);
        box.w = (s32)w;
        box.h = (s32)h;
        box.confidence = (u8)(max_score * 100);
        box.type = DETECT_TYPE_LICENSE_PLATE;

        box = box_unscale(box, image);
        box = box_rotate(box, &image->props_orig, CW);

        list_add(&boxes, &box);
    }

    ncnn_mat_destroy(out0);
    ncnn_extractor_destroy(ex);
    ncnn_mat_destroy(input);

    boxes = non_maximum_suppression(boxes, threshold_nms);

    return boxes;
}

List detect_nudity(Arena *arena, Image *image, f32 threshold_confidence, f32 threshold_nms)
{
    List boxes = list_create(arena, sizeof(Box));

    ncnn_mat_t input = ncnn_mat_from_pixels((const u8 *)image->data, NCNN_MAT_PIXEL_RGB, image->props.w, image->props.h, image->props.w*3, 0);

    const f32 mean[] = {0.0f, 0.0f, 0.0f};
    const f32 norm[] = {1.0f/255.0f, 1.0f/255.0f, 1.0f/255.0f};
    ncnn_mat_substract_mean_normalize(input, mean, norm);

    ncnn_net_t net = model_nudity.net;
    ncnn_extractor_t ex = ncnn_extractor_create(net);

    ncnn_extractor_input_index(ex, NUDITY_IN0, input);

    ncnn_mat_t out0;
    ncnn_extractor_extract_index(ex, NUDITY_OUT0, &out0);

    const f32 *data      = (const f32 *)ncnn_mat_get_data(out0);
    s32 num_anchors      = ncnn_mat_get_w(out0); // 2100

    const f32 *cx_data = data + 0 * num_anchors;
    const f32 *cy_data = data + 1 * num_anchors;
    const f32 *w_data  = data + 2 * num_anchors;
    const f32 *h_data  = data + 3 * num_anchors;

    for(s32 i = 0; i < num_anchors; ++i)
    {
        f32 max_score   = 0.0f;
        s32 best_class  = -1;

        for(s32 c = 0; c < ARRAY_COUNT(nudity_censor_classes); ++c)
        {
            s32 class_idx = nudity_censor_classes[c];
            f32 s = data[(4 + class_idx) * num_anchors + i];
            if(s > max_score)
            {
                max_score  = s;
                best_class = class_idx;
            }
        }

        if(max_score < threshold_confidence || best_class < 0)
            continue;

        f32 cx = cx_data[i];
        f32 cy = cy_data[i];
        f32 w  = w_data[i];
        f32 h  = h_data[i];

        Box box      = {0};
        box.x        = (s32)(cx - w * 0.5f);
        box.y        = (s32)(cy - h * 0.5f);
        box.w        = (s32)w;
        box.h        = (s32)h;
        box.confidence = (u8)(max_score * 100);
        box.type     = DETECT_TYPE_NUDITY;

        box = box_unscale(box, image);
        box = box_rotate(box, &image->props_orig, CW);

        list_add(&boxes, &box);
    }

    ncnn_mat_destroy(out0);
    ncnn_extractor_destroy(ex);
    ncnn_mat_destroy(input);

    boxes = non_maximum_suppression(boxes, threshold_nms);

    return boxes;
}

Box box_unscale(Box box, Image *image)
{
    const f32 scale_factor = image->props.scale == 0.0f ? 1.0f : 1.0f / image->props.scale;

    b32 swap_dimensions = (image->props_orig.rotation == ROTATE_90 || image->props_orig.rotation == ROTATE_270);

    s32 src_w = swap_dimensions ? image->props_orig.h : image->props_orig.w;
    s32 src_h = swap_dimensions ? image->props_orig.w : image->props_orig.h;

    box.x = (s32)(MAX(0,(s32)(box.x - image->props.pad_x)) * scale_factor);
    box.y = (s32)(MAX(0,(s32)(box.y - image->props.pad_y)) * scale_factor);
    box.w = (s32)(box.w * scale_factor);
    box.h = (s32)(box.h * scale_factor);

    for(s32 k = 0; k < LANDMARK_COUNT; k++)
    {
        Point *lm = &box.landmarks[k];

        lm->x = (s32)(MAX(0,lm->x - image->props.pad_x) * scale_factor);
        lm->y = (s32)(MAX(0,lm->y - image->props.pad_y) * scale_factor);
    }

    return box;
}

Box box_rotate(Box box, ImageProps *props, ClockDir dir)
{
    if(props->rotation == ROTATE_0) return box;

    b32 swap_dimensions = (props->rotation == ROTATE_90 || props->rotation == ROTATE_270);

    s32 src_w = swap_dimensions ? props->h : props->w;
    s32 src_h = swap_dimensions ? props->w : props->h;

    // normalize to CW
    Rotation effective = props->rotation;
    if(dir == CCW)
    {
        switch(props->rotation)
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

Box box_pad(Box box, ImageProps *props, f32 padding_percent)
{
    Box padded = box;

    s32 pad_x = (s32)(box.w * padding_percent);
    s32 pad_y = (s32)(box.h * padding_percent);

    padded.x = box.x - pad_x;
    padded.y = box.y - pad_y;
    padded.w = box.w + 2*pad_x;
    padded.h = box.h + 2*pad_y;

    padded.x = CLAMP(padded.x, 0, (s32)(props->w - 1));
    padded.y = CLAMP(padded.y, 0, (s32)(props->h - 1));
    padded.w = CLAMP(padded.w, 1, (s32)(props->w - padded.x - 1));
    padded.h = CLAMP(padded.h, 1, (s32)(props->h - padded.y - 1));

    return padded;
}


void box_print(Box *b, LogLevel ll)
{
    if(!b) return;

    os_log(ll, __FILE__, __LINE__, "Box: (" STR_FMT ") [%-4d %-4d %-4d %-4d], Confidence: %2u, Landmarks: [(%-4d,%-4d),(%-4d,%-4d),(%-4d,%-4d),(%-4d,%-4d),(%-4d,%-4d)]",
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
        case DETECT_TYPE_FACE_10G:      return S("face_10g");
        case DETECT_TYPE_PERSON:        return S("person");
        case DETECT_TYPE_LICENSE_PLATE: return S("license_plate");
        case DETECT_TYPE_NUDITY:        return S("nudity");
        case DETECT_TYPE_EYE:           return S("eye");
        case DETECT_TYPE_NOSE:          return S("nose");
        case DETECT_TYPE_MOUTH:         return S("mouth");
        case DETECT_TYPE_CHEEK:         return S("cheek");
        case DETECT_TYPE_FOREHEAD:      return S("forehead");
        case DETECT_TYPE_NONE:
        default: break;
    }

    return S("none");
}

DetectType detect_type_from_string(String str)
{
    if(string_equal(str, S("face")))
        return DETECT_TYPE_FACE;

    if(string_equal(str, S("face_10g")))
        return DETECT_TYPE_FACE_10G;

    if(string_equal(str, S("person")))
        return DETECT_TYPE_PERSON;

    if(string_equal(str, S("license_plate")))
        return DETECT_TYPE_LICENSE_PLATE;

    if(string_equal(str, S("nudity")))
        return DETECT_TYPE_NUDITY;

    if(string_equal(str, S("eye")))
        return DETECT_TYPE_EYE;

    if(string_equal(str, S("nose")))
        return DETECT_TYPE_NOSE;
    
    if(string_equal(str, S("mouth")))
        return DETECT_TYPE_MOUTH;

    if(string_equal(str, S("cheek")))
        return DETECT_TYPE_MOUTH;

    if(string_equal(str, S("forehead")))
        return DETECT_TYPE_FOREHEAD;

    return DETECT_TYPE_NONE;
}

String detect_model_to_string(ModelFace model)
{
    switch(model)
    {
        case MODEL_FACE_SCRFD_2_5G: return S("scrfd_2_5g");
        case MODEL_FACE_SCRFD_10G:  return S("scrfd_10g");
        default: break;
    }

    return S("scrfd_2_5g");
}

ModelFace detect_model_from_string(String str)
{
    if(string_equal(str, S("scrfd_2_5g")))
        return MODEL_FACE_SCRFD_2_5G;

    if(string_equal(str, S("scrfd_10g")))
        return MODEL_FACE_SCRFD_10G;

    return MODEL_FACE_SCRFD_2_5G;
}

DetectReport detect_report_create(Arena *arena, s64 item_count)
{
    DetectReport report = {0};

    report.enabled              = true;
    report.item_count           = item_count;
    report.items                = PUSH_ARRAY(arena, DetectReportItem, item_count);

    return report;
}

void detect_report_init_item(DetectReport *report, s64 item_index, u64 frame_count_total, f32 fps)
{
    if(!report->enabled)
        return;

    if(item_index < 0 || item_index >= report->item_count)
        return;

    DetectReportItem *item = &report->items[item_index];
    item->frame_count_total = frame_count_total;
    item->frames_per_second = fps;
}

void detect_report_update_item(DetectReport *report, s64 item_index, BoxFrame *box_frame)
{
    if(!report->enabled)
        return;

    if(item_index < 0 || item_index >= report->item_count)
        return;

    DetectReportItem *item = &report->items[item_index];

    item->total_detect_boxes += box_frame->box_count;

    for(u32 i = 0; i < box_frame->box_count; ++i)
    {
        Box *box = &box_frame->boxes[i];

        item->sum_box_width  += box->w;
        item->sum_box_height += box->h;
        item->sum_confidence += box->confidence;
        
        if(box->confidence > item->highest_confidence)
        {
            item->highest_confidence = box->confidence;
        }
    }
}

void detect_report_print(DetectReport *report, void *settings_v, LogLevel ll)
{
    if(!report->enabled)
        return;

    Settings *settings = (Settings *)settings_v;
    DetectConfig *cfg  = &settings->detect_configs[0];

    os_log(ll, __FILE__, __LINE__, "============== Report ==============");
    os_log(ll, __FILE__, __LINE__, "Model                 %s", "");
    os_log(ll, __FILE__, __LINE__, "Confidence Threshold  %.2f", cfg->threshold_confidence);
    os_log(ll, __FILE__, __LINE__, "NMS Threshold         %.2f", cfg->threshold_nms);
    os_log(ll, __FILE__, __LINE__, "Total Asset Count     %u",   report->item_count);
    os_log(ll, __FILE__, __LINE__, "Total Detection Boxes %u",   0);
    os_log(ll, __FILE__, __LINE__, "====================================");
    os_log(ll, __FILE__, __LINE__, "| Filename | Duration | Frame Count | Total Boxes | Avg Box Width | Avg Box Height | Highest Confidence | Avg Confidence |");
    os_log(ll, __FILE__, __LINE__, "|----------|----------|-------------|-------------|---------------|----------------|--------------------|----------------|");

    for(s64 i = 0; i < report->item_count; ++i)
    {
        DetectReportItem *item = &report->items[i];
        Asset *asset = &settings->assets[i];

        f32 avg_box_width  = item->total_detect_boxes == 0 ? 0 : item->sum_box_width / (f32)item->total_detect_boxes;
        f32 avg_box_height = item->total_detect_boxes == 0 ? 0 : item->sum_box_height / (f32)item->total_detect_boxes;
        f32 avg_confidence = item->total_detect_boxes == 0 ? 0 : item->sum_confidence / (f32)item->total_detect_boxes;

        os_log(ll, __FILE__, __LINE__, "|" STR_FMT "|%f|%u|%u|%f|%f|%f|%f|",
                STR_ARG(asset->path),
                item->frames_per_second == 0.0 ? 0.0 : item->frame_count_total / item->frames_per_second,
                item->frame_count_total,
                item->total_detect_boxes,
                avg_box_width,
                avg_box_height,
                item->highest_confidence,
                avg_confidence
        );
    }

    os_log(ll, __FILE__, __LINE__, "====================================");
}
