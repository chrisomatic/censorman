#include <stdio.h>

#include "base.h"
#include "os.h"

#include "detect.h"
#include "ffmpeg.h"
#include "transform.h"
#include "util.h"

#define CENSORMAN_VERSION 2

// TODO
// [ ] Add padding to sub-images
// [ ] Add lots of test images and --tester mode
// [ ] Implement thread pool (mutex vs spin-lock)
// [ ] Add audio stream encoding (1:1)
// [ ] Add a output_size CLI parameter
// [ ] Statically compile ncnn (Tencent) inference engine
// [ ] Optimize memory usage for --no_encoding
// [ ] Add YuNet model (.bin and .param) to compare accuracy
// [ ] Add --model_dir parameter for plugin system
// [ ] Add plugin

// DONE
// [x] Thread the transformations
// [x] Add Return Code Enum for any errors and success
// [x] Implement --out_file settings
// [x] Added crude first pass to address video discontinuities
// [x] Fix builds for MacOS
// [x] Get Windows support working
// [x] Clean up CLI interface and standard output
// [x] Use an exponential smooth instead of lerp for video frames
// [x] Fix 'Could not find ref with POC' bug with some videos
// [x] Add 'frame_smoothing_window' parameter (default: 150ms)
// [x] Add in optimized box blur in place of Gaussian
// [x] Fix count in video bbx output
// [x] Fix final frame missing from detection bug
// [x] Fix pixelate transform
// [x] Fix raster font on rotated videos
// [x] Add raster font for debug output
// [x] Lerping boxes in video
// [x] Add scramble transform
// [x] Add blur transform
// [x] Add image scaling function
// [x] Open a video file and read image frames
// [x] Write output video file

extern Timer timer;
extern Arena* thread_arenas[MAX_ARENAS];
extern Image texture_image;

Arena *thread_arenas[MAX_ARENAS] = {};
Arena *frame_arena = NULL;
Arena *perm_arena = NULL;

Timer timer = {};
ProgramSettings settings = {};
Image texture_image = {};
f64 begin_time = 0.0;

CM_RetCode init(s32 argc, char **args);
CM_RetCode handle_image();
CM_RetCode handle_video();

b32 parse_args(ProgramSettings* settings, s32 argc, char* argv[]);

int main(s32 argc, char** args)
{
    CM_RetCode ret;

    ret = init(argc, args);
    if(ret != CM_SUCCESS)
        return ret;

    if(settings.asset_type == TYPE_IMAGE)
    {
        ret = handle_image();
    }
    else if(settings.asset_type == TYPE_VIDEO)
    {
        ret = handle_video();
    }

    return ret;
}

CM_RetCode handle_image()
{
    Image image = {};

    CM_RetCode ret = CM_SUCCESS;

    ArenaTemp scratch = scratch_begin();

    for(s32 i = 0; i < settings.input_file_count; ++i)
    {
        String infile;
        infile = string_concat(scratch.arena, 3, settings.input_directory, S("/"), settings.input_files[i].filename);

        logv("infile: " STR_FMT, STR_ARG(infile));

        char *cstr = string_to_cstr(scratch.arena, infile);

        b32 loaded = util_load_image(cstr, &image);
        if(!loaded)
        {
            loge("Failed to open image file: %s", cstr);
            ret = CM_FAILED_OPEN_FILE;   
            continue;
        }

        OS_File bbx_file = {};

        if(settings.has_bbx_output)
        {
            // open bbx file for output
            char *bbx_cstr = string_to_cstr(scratch.arena, settings.bbx_output);
            bbx_file = os_file_open_writeonly(bbx_cstr);

            if(!bbx_file.is_valid)
            {
                logw("Failed to open Bounding Boxes file for writing " STR_FMT, STR_ARG(settings.bbx_output));
                ret = CM_FAILED_BBX_OPEN;
            }
            else
            {
                // write file header
                os_file_write_str(bbx_file, S("BBX"));
                os_file_write_u8(bbx_file,  BBX_VERSION);
                os_file_write_u16(bbx_file, (u16)image.w);
                os_file_write_u16(bbx_file, (u16)image.h);
                os_file_write_f32(bbx_file, 0.0);
                os_file_write_u32(bbx_file, 1); // only one frame for an image
            }
        }

        Image image_scaled = {};
        s32 scaled_size = settings.scaled_size_image;
        b32 use_scaled_image = false;

        if(!settings.no_scale)
        {
            stopwatch_start();
            use_scaled_image = transform_downscale(NULL, &image,&image_scaled,scaled_size,0); // @TODO: rotation
            logv("Downscale took %.3f ms", stopwatch_time()*1000.0);
            //util_write_output(&image_scaled, S("output/out_scaled.png"));
        }

        Box boxes[256] = {};
        s32 num_boxes = use_scaled_image ? process_image(&image_scaled, boxes) : process_image(&image, boxes);
        logv("Found %d boxes", num_boxes);

        if(use_scaled_image)
        {
            // correct boxes positions / sizes
            const f64 scale = image.w > image.h ? image.w / (f64)image_scaled.w : image.h / (f64)image_scaled.h;

            for(s32 i = 0; i < num_boxes; ++i)
            {
                Box* r = &boxes[i];

                r->x = (s32)round(r->x * scale);
                r->y = (s32)round(r->y * scale);
                r->w = (s32)round(r->w * scale);
                r->h = (s32)round(r->h * scale);

                for(s32 j = 0; j < 5; ++j)
                {
                    r->landmarks[j].x = (s32)round(r->landmarks[j].x * scale);
                    r->landmarks[j].y = (s32)round(r->landmarks[j].y * scale);
                }

                // add padding if needed
                const f32 box_pad_pct = settings.box_padding_pct;

                f32 sw = (f32)r->w * box_pad_pct;
                f32 sh = (f32)r->h * box_pad_pct;

                r->x -= (s32)(sw/2.0);
                r->y -= (s32)(sw/2.0);
                r->w += sw;
                r->h += sh;

                if(r->x < 0)             r->x = 0;
                if(r->x >= image.w)      r->x = image.w-1;
                if(r->y < 0)             r->y = 0;
                if(r->y >= image.h)      r->y = image.h-1;
                if(r->x+r->w >= image.w) r->w = (image.w-r->x-1);
                if(r->y+r->h >= image.h) r->h = (image.h-r->y-1);
            }
        }

        if(bbx_file.is_valid)
        {
            os_file_write_u32(bbx_file, 0); // frame index
            os_file_write_u16(bbx_file, num_boxes);

            for(s32 i  = 0; i < num_boxes; ++i)
            {
                util_write_bbx_to_file(bbx_file, &boxes[i]);
            }
        }

        for(s32 i = 0; i < settings.transform_count; ++i)
        {
            Transform* t = &settings.transforms[i];
            logv("Applying %s transform...", transform_type_to_str(t->type));
            transform_apply(&image, num_boxes, boxes,t->type);
        }

        if(settings.debug)
        {
            draw_debugging_info(&image, boxes, num_boxes);
        }

        if(!settings.no_encoding)
        {
            b32 multiple_files = (settings.input_file_count > 1);
            b32 specified_output = (settings.output_file_path.len > 0);

            String outfile = {0};
            if(!multiple_files && specified_output)
            {
                outfile = settings.output_file_path;
            }
            else
            {
                logv("input_files[%d] filename: " STR_FMT, i, STR_ARG(settings.input_files[i].filename));
                outfile = string_concat(scratch.arena, 3, settings.output_directory, S("/"), settings.input_files[i].filename);
            }
            
            logi("outfile %d: " STR_FMT, i, STR_ARG(outfile));
            b32 write_success = util_write_output(&image, outfile);
            if(!write_success)
            {
                loge("Failed to write output file");
                ret = CM_FAILED_WRITE_OUTPUT;
            }
        }

        if(bbx_file.is_valid)
        {
            os_file_close(bbx_file);
        }
    }

    scratch_end(scratch);

    return ret;
}

CM_RetCode handle_video()
{
    Arena *arena_results = arena_create(MB(16));
    if(!arena_results) return CM_FAILED_ARENA_CREATE;

    logi("Opening video file '" STR_FMT "'...", STR_ARG(settings.input_file_text));
    
    Video vid = {};
    VideoCtx vid_ctx = {};

    stopwatch_start();

    ArenaTemp scratch = scratch_begin();

    String out_file;
    b32 specified_output = (settings.output_file_path.len > 0);
    if(specified_output)
    {
        out_file = settings.output_file_path;
    }
    else
    {
        out_file = string_concat(scratch.arena, 3, settings.output_directory, S("/"), settings.input_files[0].filename);
    }

    logi("Opening video file at " STR_FMT, STR_ARG(out_file));

    char *out_file_cstr = string_to_cstr(scratch.arena, out_file);

    // open video file for streaming
    b32 opened = ffmpeg_open(perm_arena, settings.input_file_text, out_file_cstr, &vid, &vid_ctx);
    if(!opened)
    {
        loge("Failed to open stream for video " STR_FMT, STR_ARG(settings.input_file_text));
        return CM_FAILED_OPEN_FILE;
    }

    scratch_end(scratch);

    CM_RetCode ret = CM_SUCCESS;

    logi("Opened. [%6.3f s]", stopwatch_time());

    OS_File bbx_file = {};
    if(settings.has_bbx_output)
    {
        // open bbx file for output
        char *bbx_output_cstr = string_to_cstr(perm_arena, settings.bbx_output);
        bbx_file = os_file_open_writeonly(bbx_output_cstr);
        if(!bbx_file.is_valid)
        {
            logw("Failed to open Bounding Boxes file for writing " STR_FMT, STR_ARG(settings.bbx_output));
            ret = CM_FAILED_BBX_OPEN;
        }
        else
        {
            // write file header
            os_file_write_str(bbx_file, S("BBX"));
            os_file_write_u8(bbx_file,  BBX_VERSION);
            os_file_write_u16(bbx_file, (u16)vid.w);
            os_file_write_u16(bbx_file, (u16)vid.h);
            os_file_write_f32(bbx_file, (f32)vid.fps);
            os_file_write_u32(bbx_file, 0x00000000); // stub for frame count
        }
    }

    Image* images        = (Image*)calloc(settings.thread_count, sizeof(Image));
    Image* images_scaled = (Image*)calloc(settings.thread_count, sizeof(Image));

    f64 total_time_decoding = 0.0;
    f64 total_time_detecting = 0.0;
    f64 total_time_transforming = 0.0;
    f64 total_time_encoding = 0.0;

    u32 total_frames_processed = 0;

    for(;;)
    {
        logi("Decoding Chunk...");
        arena_reset(frame_arena);
        stopwatch_start();

        // decode data
        b32 decoded = ffmpeg_decode_ctx(&vid, &vid_ctx);
        if(!decoded)
        {
            loge("Failed to decode video " STR_FMT, STR_ARG(settings.input_file_text));
            ret = CM_FAILED_VIDEO_DECODE;
            break;
        }

        f64 decode_time = stopwatch_time();
        total_time_decoding += decode_time;
        logi("Decoded Chunk (Size: %d). [%6.3f s] ", vid.frame_count, decode_time);

        // If no frames were decoded, we are done
        if(vid.frame_count == 0 && vid.decode_complete)
        {
            break;
        }

        // run detections

        logi("Detecting faces...");
        stopwatch_start();

        memset(images, 0, settings.thread_count*sizeof(Image));
        memset(images_scaled, 0, settings.thread_count*sizeof(Image));

        b32 *use_scaled = (b32 *)PUSH_ARRAY(frame_arena, b32, settings.thread_count);
        u8 *detect_buffers = (u8 *)PUSH_ARRAY(frame_arena, u8, 0x9000 * settings.thread_count);

        u32 output_count = 0;
        u8* output_ptrs[4096] = {};

        s32 frame_counter = 0;
        s32 actual_thread_count;

        s32 skip_frames = MAX(1, (s32)(settings.frame_smoothing_window * vid.fps));
        logv("Number of skip frames: %d (Based on %f smoothing window)", skip_frames, settings.frame_smoothing_window);

        u32 *detect_frames = (u32 *)PUSH_ARRAY(frame_arena, u32, vid.frame_count);
        u32 detect_frames_count = 0;
        u32 _counter = 0;

        for(;;)
        {
            detect_frames[detect_frames_count++] = _counter;

            s32 frame_advance = MIN(skip_frames, vid.frame_count - _counter - 1);
            if(frame_advance <= 0)
                break;

            _counter += frame_advance; // always want to evaluate final frame
        }

        scratch = scratch_begin();

        {
            // determine any video discontinuities
            // ref: https://www-nlpir.nist.gov/projects/tvpubs/tvpapers03/ramonlull.paper.pdf
            logi("Determining video discontinuities...");

            u32 prev_histogram[4096] = {0};
            u32 curr_histogram[4096] = {0};
            u64 *diff_histogram = (u64 *)PUSH_ARRAY(scratch.arena, u64, vid.frame_count);

            // get color histogram for each frame of video
            // and compute a absolute difference in histogram values (summed)
            // for all video frames
            for(s32 i = 0; i < vid.frame_count; ++i)
            {
                s32 icurr = (u64)i*vid.w*vid.h*3;
                u8 *bcurr = &vid.data[icurr];

                if(i > 0)
                {
                    // copy curr histogram to prev
                    MemoryCopy(prev_histogram, curr_histogram, sizeof(u32)*4096);
                    MemoryZero(curr_histogram, sizeof(u32)*4096);
                }

                // go through each pixel and compute a difference image
                for(s32 j = 0; j < vid.w*vid.h; ++j)
                {
                    s32 idx = j*3;

                    u8 r_curr = bcurr[idx+0];
                    u8 g_curr = bcurr[idx+1];
                    u8 b_curr = bcurr[idx+2];

                    // get upper 4 bits from R,G,B channels and
                    // combine into a 12-bit number (4096 possible values) (stored in u16)
                    // this is the index into the histogram
                    u16 color_bucket = ((u16)(r_curr & 0xF0) << 4) | (u16)(g_curr & 0xF0) | (u16)(b_curr & 0x0F);
                    curr_histogram[MIN(color_bucket, 4095)]++;
                }

                u64 sum_histogram = 0;
                for(u32 j = 0; j < 4096; ++j)
                {
                    sum_histogram += ABS((s64)curr_histogram[j] - prev_histogram[j]);
                }

                diff_histogram[i] = sum_histogram;
            }

            // 4096 buckets that sum up to vid.w * vid.h
            // A maximum difference between frames is vid.w * vid.h
            // perhaps a reasonable change from frame to frame would be 20% changed
            // to consider it a frame that should be scheduled for detection

            u64 threshold = (u64)((vid.w * vid.h) * 0.2f);

            for(s32 i = 1; i < vid.frame_count; ++i) // don't consider first frame
            {
                if(diff_histogram[i] >= threshold)
                {
                    b32 in_array = false;
                    for(s32 j = 0; j < detect_frames_count; ++j)
                    {
                        if(detect_frames[j] == i)
                        {
                            in_array = true;
                            break;
                        }
                    }
                    if(!in_array)
                    {
                        detect_frames[detect_frames_count++] = i;
                    }
                }
            }
        }

        scratch_end(scratch);

        ARRAY_SORT(detect_frames, u32, detect_frames_count, false);

        u32 detect_frames_index = 0;
        frame_counter = detect_frames[detect_frames_count++];

        for(;;)
        {
            for(s32 i = 0; i < settings.thread_count; ++i)
            {
                memset(&images_scaled[i], 0, sizeof(Image));
                memset(detect_buffers+(0x9000*i), 0, 0x9000);
                use_scaled[i] = false;
            }

            if(detect_frames_index >= detect_frames_count)
                break;

            actual_thread_count = 0;

            // Create threads
            for(s32 i = 0; i < settings.thread_count; ++i)
            {
                Arena* arena = thread_arenas[actual_thread_count];
                arena_reset(arena);

                Image* image = &images[actual_thread_count];
                Image* image_scaled = &images_scaled[actual_thread_count];

                // fill out the image object
                image->detect_buffer = (detect_buffers + (0x9000 * actual_thread_count));
                image->frame_number = frame_counter;
                image->data = &vid.data[(u64)frame_counter*vid.w*vid.h*3];
                image->w = vid.w;
                image->h = vid.h;
                image->n = 3;
                image->step = 3*image->w;
                image->arena = arena;

                if(!settings.no_scale)
                {
                     // Scale down image
                    s32 scaled_size = settings.scaled_size_video;
                    use_scaled[actual_thread_count] = transform_downscale(arena, image,image_scaled,scaled_size, vid.rotation);
                }

                reverse_rgb_order(use_scaled[i] ? image_scaled : image);

                // start detection thread
                if(thread_create(&threads[actual_thread_count], detect_faces, (void*)(use_scaled[i] ? image_scaled : image)) == 0)
                {
                    actual_thread_count++;
                }
                else
                {
                    logw("Failed to start thread");
                    ret = CM_FAILED_THREAD_CREATE;
                }

                frame_counter = detect_frames[detect_frames_index++];
            }

            logv("Joining Threads.");
            
            // join all threads back
            for(s32 i = 0; i < actual_thread_count; ++i)
            {
                logv("Joining Thread %d...", i);
                thread_join(threads[i]);
            }

            logv("Threads joined!");
            
            // Gather results

            s32 num_faces = 0;

            //arena_reset(arena_results);

            for(s32 i = 0; i < actual_thread_count; ++i)
            {
                Image* image = use_scaled[i] ? &images_scaled[i] : &images[i];

                num_faces = 0;

                if(image && image->result)
                {
                    u8* ret_boxes = image->result;

                    s32 offset = 0;
                    s32 _faces_found = *((s32*)(ret_boxes));
                    offset += sizeof(s32);

                    logv("frame number: %d, faces_found: %d", image->frame_number, _faces_found);
                    output_ptrs[image->frame_number] = (u8 *)PUSH_ARRAY(arena_results, u8, sizeof(u32)+(_faces_found*sizeof(Box)));

                    for(s32 j = 0; j < _faces_found; ++j)
                    {
                        Box* r = (Box*)(ret_boxes+offset);

                        if(use_scaled[i])
                        {
                            const f32 scale = vid.w > vid.h ? vid.w / (f32)image->w : vid.h / (f32)image->h;
                            transform_box_upscale_rotate_inverse(r, image->w, image->h, vid.w, vid.h, image->rotation);
                        }

                        memcpy(output_ptrs[image->frame_number]+offset, r, sizeof(Box)); offset += sizeof(Box);
                        num_faces++;
                    }

                    memcpy(output_ptrs[image->frame_number], &num_faces, sizeof(u32));
                    output_count += num_faces;

                    reverse_rgb_order(image);
                }
            }

            logv("[Frame %d/%d]: num_faces: %d", frame_counter+1, vid.frame_count, num_faces);
        }

        f64 detect_time = stopwatch_time();
        total_time_detecting += detect_time;
        logi("Detection done. [%6.3f s]", detect_time);
        
        //  
        //  |------|------|------|
        // rl0     x      x     rl1
        //

        BoxList rl0 = {};
        BoxList rl1 = {};

        for (s32 i = 0; i < vid.frame_count; )
        {
            u8 *ptr = output_ptrs[i];

            // rl0 <- previous frame
            memcpy(&rl0, &rl1, sizeof(BoxList));

            if (ptr)
            {
                // valid frame, rl1 <- current frame
                memcpy(&rl1.box_count, ptr, sizeof(u32));
                rl1.boxes = (Box*)(ptr + sizeof(u32));

                i++; // advance to next frame
                continue;
            }
            else
            {
                // Gap frame — look ahead to find next valid frame
                s32 j = i + 1;
                while (j < vid.frame_count && !output_ptrs[j])
                    j++;

                s32 frames_in_between = j - i;

                // If no future valid frame, copy rl0 forward for remaining frames
                if (j >= vid.frame_count)
                {
                    for (s32 f = 0; f < frames_in_between; ++f)
                    {
                        // allocate space for output_ptrs
                        s32 rc = rl0.box_count;
                        output_ptrs[i+f] = (u8*)PUSH_ARRAY(arena_results, u8, sizeof(u32)+(rc*sizeof(Box)));
                        memcpy(output_ptrs[i+f], &rc, sizeof(u32));
                        memcpy(output_ptrs[i+f] + sizeof(u32), rl0.boxes, rc*sizeof(Box));
                    }
                    break; // reached end of video
                }

                // Set rl1 to the next valid frame
                memcpy(&rl1.box_count, output_ptrs[j], sizeof(u32));
                rl1.boxes = (Box*)(output_ptrs[j] + sizeof(u32));

                // Now interpolate frames between rl0 and rl1
                for (s32 f = 0; f < frames_in_between; ++f)
                {
                    // Decide which box list is bigger
                    BoxList *a = (rl0.box_count >= rl1.box_count) ? &rl0 : &rl1;
                    BoxList *b = (rl0.box_count >= rl1.box_count) ? &rl1 : &rl0;

                    s32 rc = a->box_count;
                    u8 *out = (u8*)PUSH_ARRAY(arena_results, u8, sizeof(u32)+(rc*sizeof(Box)));
                    memcpy(out, &rc, sizeof(u32));

                    s32 offset = sizeof(u32);

                    s32 matched_count = 0;
                    s32 matches[256] = {};

                    // Exponential smoothing / lerp for matched boxes
                    for (s32 k = 0; k < a->box_count; ++k)
                    {
                        f32 min_mv = FLT_MAX;
                        s32 min_index = -1;

                        for (s32 l = 0; l < b->box_count; ++l)
                        {
                            b32 already_matched = false;
                            for (s32 m = 0; m < matched_count; ++m)
                                if (matches[m] == l) { already_matched = true; break; }
                            if (already_matched) continue;

                            Box *ra = &a->boxes[k];
                            Box *rb = &b->boxes[l];

                            f32 dx = ABS(ra->x - rb->x);
                            f32 dy = ABS(ra->y - rb->y);
                            f32 dw = ABS(ra->w - rb->w);
                            f32 dh = ABS(ra->h - rb->h);

                            f32 mv = dx + dy + dw + dh;
                            if (mv < min_mv)
                            {
                                min_mv = mv;
                                min_index = l;
                            }
                        }

                        Box *rg = (Box*)(out + offset);
                        offset += sizeof(Box);

                        if (min_index >= 0)
                        {
                            // mark as matched
                            matches[matched_count++] = min_index;

                            Box *ra = &a->boxes[k];
                            Box *rb = &b->boxes[min_index];

                            const f32 alpha = 0.2f; // smoothing
                            rg->x = (s32)exponential_smooth((f32)ra->x, (f32)rb->x, alpha, f);
                            rg->y = (s32)exponential_smooth((f32)ra->y, (f32)rb->y, alpha, f);
                            rg->w = (s32)exponential_smooth((f32)ra->w, (f32)rb->w, alpha, f);
                            rg->h = (s32)exponential_smooth((f32)ra->h, (f32)rb->h, alpha, f);
                            rg->confidence = (s32)exponential_smooth((f32)ra->confidence, (f32)rb->confidence, alpha, f);

                            for (s32 j2 = 0; j2 < 5; ++j2)
                            {
                                rg->landmarks[j2].x = (s32)exponential_smooth((f32)ra->landmarks[j2].x, (f32)rb->landmarks[j2].x, alpha, f);
                                rg->landmarks[j2].y = (s32)exponential_smooth((f32)ra->landmarks[j2].y, (f32)rb->landmarks[j2].y, alpha, f);
                            }
                        }
                        else
                        {
                            // No match, just copy forward from a
                            memcpy(rg, &a->boxes[k], sizeof(Box));
                        }
                    }

                    // Copy output
                    output_ptrs[i+f] = out;
                }

                // Advance i past interpolated frames
                i += frames_in_between;
            }
        }

        s32 zero_boxes_frames = 0;

        // perform transformations
        logi("Applying transformations...");
        stopwatch_start();

        logv("Iterating through video frames (Total: %d)", vid.frame_count);

        {
            TransformThreadData *thread_datas = (TransformThreadData *)malloc(settings.thread_count * sizeof(TransformThreadData));

            u64 frame_counter = 0;
            
            for(;;)
            {
                if(frame_counter >= vid.frame_count)
                    break;

                actual_thread_count = 0;
                memset(images, 0, settings.thread_count*sizeof(Image));
                memset(thread_datas, 0, settings.thread_count*sizeof(TransformThreadData));

                // Create threads
                for(s32 i = 0; i < settings.thread_count; ++i)
                {
                    // Get frame
                    Image* image = &images[actual_thread_count];
                    Arena* arena = thread_arenas[actual_thread_count];

                    image->frame_number = frame_counter;
                    image->data = &vid.data[(u64)frame_counter*vid.w*vid.h*3];
                    image->w = vid.w;
                    image->h = vid.h;
                    image->n = 3;
                    image->step = 3*image->w;
                    image->arena = arena;

                    // Get Boxes
                    u8 *ptr = output_ptrs[frame_counter];

                    if(!ptr)
                    {
                        logv("No output at Frame %d", frame_counter);
                        frame_counter++;
                        continue;
                    }

                    u32 num_boxes = 0;
                    MemoryCopy((u8*)&num_boxes, ptr, sizeof(u32));
                    ptr += sizeof(u32);
                    Box *boxes = (Box *)(ptr);

                    if(bbx_file.is_valid)
                    {
                        os_file_write_u32(bbx_file, total_frames_processed+frame_counter); // frame index
                        os_file_write_u16(bbx_file, num_boxes);
                    }

                    // add padding if needed
                    const f32 box_pad_pct = settings.box_padding_pct;

                    for(s32 j = 0; j < num_boxes; ++j)
                    {
                        f32 sw = (f32)boxes[j].w * box_pad_pct;
                        f32 sh = (f32)boxes[j].h * box_pad_pct;

                        boxes[j].x -= (s32)(sw/2.0);
                        boxes[j].y -= (s32)(sw/2.0);
                        boxes[j].w += sw;
                        boxes[j].h += sh;

                        b32 no_rotate = (settings.no_rotate && (vid.rotation == 90 || vid.rotation == 270));
                        s32 w = no_rotate ? image->h : image->w;
                        s32 h = no_rotate ? image->w : image->h;

                        if(boxes[j].x < 0)  boxes[j].x = 0;
                        if(boxes[j].x >= w) boxes[j].x = w-1;
                        if(boxes[j].y < 0)  boxes[j].y = 0;
                        if(boxes[j].y >= h) boxes[j].y = h-1;
                        if(boxes[j].x+boxes[j].w >= w) boxes[j].w = (w-boxes[j].x-1);
                        if(boxes[j].y+boxes[j].h >= h) boxes[j].h = (h-boxes[j].y-1);

                        if(bbx_file.is_valid)
                        {
                            util_write_bbx_to_file(bbx_file, &boxes[j]);
                        }

                        logv("[Frame %d][Box %d] %u %u %u %u (%u%)", frame_counter, j, boxes[j].x, boxes[j].y, boxes[j].w, boxes[j].h, boxes[j].confidence);
                    }

                    if(num_boxes == 0)
                    {
                        zero_boxes_frames++;
                    }

                    TransformThreadData *thread_data = &thread_datas[actual_thread_count];

                    thread_data->image = image;
                    thread_data->num_boxes = num_boxes;
                    thread_data->boxes = boxes;

                    logv("Creating Thread %d", actual_thread_count);
                    if(thread_create(&threads[actual_thread_count], transform_apply_threaded, (void*)thread_data) == 0)
                    {
                        actual_thread_count++;
                    }
                    else
                    {
                        logw("Failed to start thread");
                        ret = CM_FAILED_THREAD_CREATE;
                    }

                    frame_counter++;
                }

                // join all threads back
                for(s32 i = 0; i < actual_thread_count; ++i)
                {
                    thread_join(threads[i]);
                }
            }
        }
#endif

        logv("Number of zero box frames: %d", zero_boxes_frames);

        f64 transform_time = stopwatch_time();
        total_time_transforming += transform_time;
        logi("Transformations done. [%6.3f s]", transform_time);

        total_frames_processed += vid.frame_count;
        if(bbx_file.is_valid)
        {
            fsync(bbx_file.handle);
        }

        if(!settings.no_encoding)
        {
            logi("Encoding chunk...");
            stopwatch_start();
            b32 encoded = ffmpeg_encode_ctx(&vid, &vid_ctx);
            if(!encoded)
            {
                loge("Failed to write output file");
                ret = CM_FAILED_VIDEO_ENCODE;
                break;
            }

            f64 encode_time = stopwatch_time();
            total_time_encoding += encode_time;
            logi("Encode done. [%6.3f s]", encode_time);
        }

        logi("Progress: %d / %d [%d%]", total_frames_processed, vid.total_frame_count, (s32)(100.0f*total_frames_processed / (f32)vid.total_frame_count));

        // exit if video is done
        if(vid.decode_complete)
        {
            if(!settings.no_encoding) ffmpeg_encode_done(&vid_ctx);
            break;
        }
    }

    if(bbx_file.is_valid)
    {
        // Write the total frame count in the header after done
        os_file_write_u32_at_index(bbx_file, total_frames_processed, BBX_FRAME_COUNT_OFFSET);
        os_file_close(bbx_file);
    }

    f64 total_processing_time = total_time_decoding + total_time_detecting + total_time_transforming + total_time_encoding;
    f64 total_elapsed_time = timer_get_time() - begin_time;

    logi("Complete!");

    logi("Total Time Decoding:     %6.3f s [%02d%]", total_time_decoding, (s32)(100.0f*total_time_decoding / total_processing_time));
    logi("Total Time Detecting:    %6.3f s [%02d%]", total_time_detecting, (s32)(100.0f*total_time_detecting / total_processing_time));
    logi("Total Time Transforming: %6.3f s [%02d%]", total_time_transforming, (s32)(100.0f*total_time_transforming / total_processing_time));
    logi("Total Time Encoding:     %6.3f s [%02d%]", total_time_encoding, (s32)(100.0f*total_time_encoding / total_processing_time));
    logi("Total Time:              %6.3f s", total_elapsed_time);

    // ffmpeg_close(&vid_ctx);

    return ret;
}

CM_RetCode init(s32 argc, char **args)
{
    // init
    timer_init();
    begin_time = timer_get_time();

    log_init(0);

    time_t t;
    srand((unsigned) time(&t));

    // print title
    if(!is_quiet)
    {
        printf("[CENSORMAN V%d]\n", CENSORMAN_VERSION);
        printf("    _O_\n");
        printf("  /|-X-|\\\n");
        printf(" /  \\_/  \\\n");
        printf("    / \\\n");
        printf("  _/   \\_\n");
    }

    // set default settings
    settings.input_file_text = string_nil();
    settings.bbx_output      = string_nil();

    settings.thread_count           = MAX(1, util_get_core_count()*2); // default to num_cores
    settings.asset_type             = TYPE_IMAGE;
    settings.classification         = CLASS_FACE;
    settings.output_file_path       = string_nil();
    settings.transform_count        = 0;
    settings.debug                  = false;
    settings.confidence_threshold   = 20;
    settings.blur_strength          = 0.50;
    settings.has_texture            = false;
    settings.no_rotate              = false;
    settings.no_scale               = false;
    settings.block_scale            = 0.16;
    settings.frame_smoothing_window = 0.150;
    settings.input_file_count       = 0;
    settings.max_buffer_size        = MB(512);
    settings.box_padding_pct        = 0.15;
    settings.no_encoding            = false;
    settings.verbose                = false;
    settings.has_bbx_output         = false;
    settings.scaled_size_image      = 640;
    settings.scaled_size_video      = 320;

    b32 parse = parse_args(&settings, argc, args);
    if(!parse) return CM_FAILED_PARSE_ARGS;

    if(settings.verbose)
    {
        log_level = LOG_TYPE_VERBOSE;
    }
    
    // initialize memory arenas used in program
    for(s32 i = 0; i < settings.thread_count; ++i)
    {
        thread_arenas[i] = arena_create(MB(16));

        if(!thread_arenas[i])
        {
            return CM_FAILED_ARENA_CREATE;
        }
    }

    perm_arena = arena_create(MB(16)); // permanent
    if(!perm_arena) return CM_FAILED_ARENA_CREATE;

    frame_arena = arena_create(MB(16)); // for videos
    if(!frame_arena) return CM_FAILED_ARENA_CREATE;

    // initialize model data
    detect_init();
    
    // initialize threads
    b32 threads_ret = thread_init(settings.thread_count);
    if(!threads_ret)
    {
        return CM_FAILED_THREAD_ALLOC;
    }

    // check input
    Temp scratch = scratch_begin();

    String ext = os_path_get_extension(settings.input_file_text);

    if(ext.len == 0)
    {
        // likely dealing with an input folder
        logv("Loading images from folder " STR_FMT, STR_ARG(settings.input_file_text));

        settings.input_directory = settings.input_file_text;

        StringArray valid_exts = string_array_create(scratch.arena, 4, S("png"), S("jpg"), S("jpeg"), S("bmp"));
        StringArray file_array = os_get_files_by_extensions(perm_arena, settings.input_directory, valid_exts);

        for(u32 i = 0; i < file_array.count; ++i)
        {
            logv("File %d: " STR_FMT, i+1, STR_ARG(file_array.items[i]));
            settings.input_files[i].filename = file_array.items[i];
        }

        settings.input_file_count = file_array.count;
    }
    else
    {
        // single input file, not a folder
        String ext_lowered = string_to_lower(scratch.arena, ext);
        logv("File extension: " STR_FMT, STR_ARG(ext_lowered));

        StringArray ext_arr = string_array_create(scratch.arena, 2, S("mp4"), S("mov"));
        b32 is_video = string_in_array(ext_lowered, ext_arr);
        if(is_video) settings.asset_type = TYPE_VIDEO;
        settings.input_file_count = 1;

        settings.input_directory = os_path_get_directory(settings.input_file_text);
        settings.input_files[0].filename = os_path_get_file(settings.input_file_text);
        settings.input_file_count = 1;
    }
    
    // set up output folder if needed
    if(!settings.no_encoding)
    {
        if(settings.output_file_path.len == 0)
        {
            // no outfile specified, use default
            settings.output_directory = S("output");
        }
        else
        {
            // outfile specified
            b32 multiple_files = (settings.input_file_count > 1);
            settings.output_directory = multiple_files ? settings.output_file_path : os_path_get_directory(settings.output_file_path);
        }

        if(!os_path_is_directory(settings.output_directory))
        {
            logv("Creating directory: " STR_FMT, STR_ARG(settings.output_directory));
            os_file_create_directory(settings.output_directory);
        }
    }

    scratch_end(scratch);

    if(settings.has_texture)
    {
        b32 loaded = util_load_image(settings.texture_image_path, &texture_image);
        if(!loaded)
        {
            logw("Failed to load texture image %s", settings.texture_image_path);
            settings.has_texture = false;
        }
    }
    
    // print settings
    logi("");
    logi("=============== Settings ===============");
    logi("  File Count:             %d", settings.input_file_count);
    logi("  Asset Type:             %s", settings.asset_type == TYPE_IMAGE ? "Image" : "Video");
    logi("  Thread Count:           %d", settings.thread_count);
    logi("  Confidence Threshold:   %d", settings.confidence_threshold);
    logi("  Blur Strength:          %f", settings.blur_strength);
    logi("  Block Scale:            %f", settings.block_scale);
    logi("  Texture:                %s", settings.has_texture ? settings.texture_image_path : "(None)");
    logi("  Max Buffer Size:        %lu B", settings.max_buffer_size);
    logi("  Box Padding Percent:    %f", settings.box_padding_pct);
    logi("  Frame Smoothing Window: %f", settings.frame_smoothing_window);
    logi("  No Encoding:            %s", STR_BOOL(settings.no_encoding));
    logi("  Downscaling:            %s (%d px)", settings.no_scale ? "No" : "Yes", settings.asset_type == TYPE_IMAGE ? settings.scaled_size_image : settings.scaled_size_video);
    logi("  No Rotate:              %s", STR_BOOL(settings.no_rotate));
    logi("  Bounding Box Output:    " STR_FMT, STR_ARG(settings.bbx_output));
    logi("  Debug:                  %s", settings.debug ? "ON" : "OFF");
    logi("  Verbose:                %s", settings.verbose ? "ON" : "OFF");
    logi("========================================");
    logi("");

    return CM_SUCCESS;
}

void print_help()
{
    printf("\n[USAGE]\n");
    printf("  censorman <in_file> -o <out_file> -d {class_list} -t {transform_list} [-c confidence_threshold][-j thread_count] [--debug] [--image <texture_image_path>] [--bbx_output <bbx_output_filepath>] [--block_scale <block_scale>] [--blur_strength <blur_strength>] [--max_buffer_size <buffer_size>] [--scaled_size <scaled_size>] [--box_padding_pct <padding_pct>] [--no_scale] [--no_encoding] [--quiet] [--verbose]\n");
    printf("\n[DESCRIPTION]\n  Takes an image or video file, detects regions of human faces (for now), applies transformations on those regions and writes back an output image file\n");
    printf("\n[ARGUMENTS]\n");
    printf("  in_file:                Path to input image (or video) file (or folder) (.jpg, .png, .bmp, .mp4, .mov)\n");
    printf("  out_file:               Path to output image (or video) file (.jpg, .png, .bmp, .mp4)\n");
    printf("  class_list:             {face}\n");
    printf("  transform_list:         {pixelate, blur, blackout, scramble, texture}\n");
    printf("  confidence_threshold:   Discard any boxes lower than this (0 - 100)\n");
    printf("  thread_count:           How many threads to use to detect (default to number of cores * 2)\n");
    printf("  debug:                  Draw boxes and confidence labels on output image/video\n");
    printf("  texture_image_path:     Used with 'texture' transform\n");
    printf("  block_scale:            Value between 0.0 and 1.0. Used to scale blocks in pixelate transform\n");
    printf("  blur_strength:          Value between 0.0 and 1.0. Blur is a box blur. (Default: 0.50)\n");
    printf("  frame_smoothing_window: Smoothing window for lerping between frames of video (Default: 0.150 or 150ms)\n");
    printf("  buffer_size:            Number of bytes for video frames during conversion (Default: 1 GB)\n");
    printf("  scaled_size:            The longest dimension in pixels to scale down to (Default: 640 for images, 320 for videos)\n");
    printf("  padding_pct:            Added percentage of padding to detected boxes (Default: 0.15)\n");
    printf("  no_encoding:            Prevents writing output image or video file\n");
    printf("  bbx_output_filepath:    Bounding boxes output file. Specify if you want this file output.\n");
    printf("  no_scale:               Disables downscaling of images and videos before detections\n");
    printf("  no_rotate:              Prevents rotation happening for input frames from video, and on bounding boxes\n");
    printf("  quiet:                  Suppress standard log output\n");
    printf("  verbose:                Enable verbose log output\n");
    printf("\n");
}

b32 parse_args(ProgramSettings* settings, s32 argc, char* argv[])
{
    if(argc <= 1)
    {
        print_help();
        return false;
    }

    b32 input_file_needed = true;

    for(s32 i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            switch(argv[i][1])
            {
                case '-':
                {
                    if(STR_EQUAL(&argv[i][2],"help"))
                    {
                        print_help();
                        return false;
                    }
                    else if(STR_EQUAL(&argv[i][2],"debug"))
                        settings->debug = true;
                    else if(STR_EQUAL(&argv[i][2],"quiet"))
                        is_quiet = true;
                    else if(STR_EQUAL(&argv[i][2],"verbose"))
                        settings->verbose = true;
                    else if(STR_EQUAL(&argv[i][2],"no_encoding"))
                        settings->no_encoding = true;
                    else if(STR_EQUAL(&argv[i][2],"no_scale"))
                        settings->no_scale = true;
                    else if(STR_EQUAL(&argv[i][2],"no_rotate"))
                        settings->no_rotate = true;
                    else if(STR_EQUAL(&argv[i][2],"out_file"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            settings->output_file_path = STR(argv[i]);
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"block_scale"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            f32 f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->block_scale = f;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"blur_strength"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            f32 f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->blur_strength = f;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"frame_smoothing_window"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            f32 f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->frame_smoothing_window = f;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"image"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            strncpy(settings->texture_image_path, argv[i], 255);
                            settings->has_texture = true;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"bbx_output"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            settings->bbx_output = STR(argv[i]);
                            settings->has_bbx_output = true;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"max_buffer_size"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            u64 n = atol(argv[i]);
                            if(n > 0) settings->max_buffer_size = n;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"scaled_size"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            u32 n = atoi(argv[i]);

                            if(n > 0)
                            {
                                printf("n: %d\n", n);
                                settings->scaled_size_image = n;   
                                settings->scaled_size_video = n;
                            }
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"box_padding_pct"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            f32 f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->box_padding_pct = f;
                        }
                    }
                    else
                    {
                        logw("Unrecognized flag: %s", &argv[i][2]);
                    }
                }   break;
                case 'o':

                    break;
                case 'd':
                    break;
                case 'c':
                {
                    s32 n = atoi(argv[i+1]);
                    settings->confidence_threshold = n == 0 ? settings->confidence_threshold : n;
                }   break;
                case 't':
                {
                    if(i < argc-1)
                    {
                        // parse transforms
                        char* p = argv[i+1];
                        s32 len = strlen(p);
                        char buf[256] = {};
                        s32 bufi = 0;
                        b32 process = false;

                        for(s32 i = 0; i < len; ++i)
                        {
                            s32 c = *p++;
                            if(c == ',')
                            {
                                process = true;
                            }
                            else
                            {
                                buf[bufi++] = c;
                            }

                            if(i == len -1)
                            {
                                process = true;
                            }

                            if(process)
                            {
                                process = false;

                                TransformType type = TRANSFORM_TYPE_NONE;

                                if(STR_EQUAL(buf, "blackout"))      type = TRANSFORM_TYPE_BLACKOUT;
                                else if(STR_EQUAL(buf, "blur"))     type = TRANSFORM_TYPE_BLUR;
                                else if(STR_EQUAL(buf, "pixelate")) type = TRANSFORM_TYPE_PIXELATE;
                                else if(STR_EQUAL(buf, "scramble")) type = TRANSFORM_TYPE_SCRAMBLE;
                                else if(STR_EQUAL(buf, "texture"))  type = TRANSFORM_TYPE_TEXTURE;

                                memset(buf,0,256);
                                bufi = 0;

                                if(type != TRANSFORM_TYPE_NONE)
                                {
                                    Transform *t = &settings->transforms[settings->transform_count++];
                                    t->type = type;
                                }
                                else
                                {
                                    logw("Transform type is None (string: %s)", buf);
                                }
                            }
                        }
                    }
                } break;
                case 'j': {
                    if(i < argc-1)
                    {
                        s32 n = atoi(argv[i+1]);
                        settings->thread_count = n == 0 ? settings->thread_count : n;
                    }
                }   break;
                default:
                    logw("Unrecognized Option: %c", argv[i][1]);
                    break;
            }
        }
        else if(input_file_needed)
        {
            // assume input file
            settings->input_file_text = STR(argv[i]);
            input_file_needed = false;
        }
    }

    return true;
}
