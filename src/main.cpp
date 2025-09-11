#include <stdio.h>
#include <pthread.h>

#include "base.h"
#include "platform.h"
#include "detect.h"
#include "ffmpeg.h"
#include "transform.h"
#include "util.h"

// TODO
//
// [ ] Lerping rects in video
// [ ] Thread the transformations
// [ ] Fix builds for MacOS
// [ ] Add padding to sub-images
// [ ] Add lots of test images and --tester mode
// [ ] Implement thread pool (mutex vs spin-lock)
// [ ] Add raster font for debug output
// [x] Add scramble transform
// [x] Add blur transform
// [x] Add image scaling function
// [x] Open a video file and read image frames
// [x] Write output video file

Arena* scratch = {0};
Arena* thread_arenas[MAX_ARENAS] = {0};
Timer timer = {0};
ProgramSettings settings = {};
pthread_t *threads = NULL;
Image texture_image = {};

bool init(int argc, char **args);
bool parse_args(ProgramSettings* settings, int argc, char* argv[]);
int process_image(Image* image,Rect* ret_rects);
int handle_image();
int handle_video();

int main(int argc, char** args)
{
    bool initialized = init(argc, args);
    if(!initialized)
        return 1;

    // check input
    char ext[10] = {0};
    int ext_len = str_get_extension(settings.input_file_text, ext, 10);
    if(ext_len == 0)
    {
        // load up images from folder
        LOGI("Loading image from folder %s", settings.input_file_text);
        strncpy(settings.input_directory,settings.input_file_text, strlen(settings.input_file_text));

        String ext1 = S(".png");
        String ext2 = S(".jpg");
        String ext3 = S(".bmp");

        String exts[] = {ext1, ext2, ext3};
        
        String* files;
        int count = platform_get_files_in_folder(scratch, str_from_cstr(settings.input_directory), exts, 3, &files);

        for (int i = 0; i < count; ++i)
        {
            LOGI("File %d: %.*s", i + 1, files[i].len, files[i].data);
            strncpy(settings.input_files[i].filename, files[i].data, files[i].len);
        }
        settings.input_file_count = count;

        arena_reset(scratch);
    }
    else
    {
        // single input file, not a folder

        LOGI("File extension: %s", ext);
        bool is_video = (STR_EQUAL(ext, "mp4") || STR_EQUAL(ext, "mov") || STR_EQUAL(ext, "MP4") || STR_EQUAL(ext, "MOV"));
        if(is_video) settings.asset_type = TYPE_VIDEO;
        settings.input_file_count = 1;

        int input_text_len = strlen(settings.input_file_text);

        for (int i = input_text_len-1; i >= 0; --i)
        {
            if(settings.input_file_text[i] == '/' || settings.input_file_text[i] == '\\')
            {
                strncpy(settings.input_files[0].filename,&settings.input_file_text[i+1],MIN(100,input_text_len - i));
                strncpy(settings.input_directory, &settings.input_file_text[0], i);
                break;
            }
        }
    }

    // initialize threads
    threads = (pthread_t *)calloc(settings.thread_count,sizeof(pthread_t));

    if(settings.has_texture)
    {
        bool loaded = util_load_image(settings.texture_image_path, &texture_image);
        if(!loaded)
        {
            LOGW("Failed to load texture image %s", settings.texture_image_path);
            settings.has_texture = false;
        }
    }

    if(settings.asset_type == TYPE_IMAGE)
    {
        handle_image();
    }
    else if(settings.asset_type == TYPE_VIDEO)
    {
        handle_video();
    }

    return 0;
}

int handle_image()
{
    Image image = {};

    for(int i = 0; i < settings.input_file_count; ++i)
    {
        String infile;
        infile = StringFormat(scratch, "%s/%s", settings.input_directory, settings.input_files[i].filename);

        LOGI("infile: %.*s", infile.len, infile.data);

        bool loaded = util_load_image(infile.data, &image);
        if(!loaded) return 1;

        Image image_scaled = {};
        const int scaled_size = 640;
        bool use_scaled_image = false;

        if(!settings.no_scale)
        {
            double t0 = timer_get_time();
            use_scaled_image = transform_downscale(NULL, &image,&image_scaled,scaled_size);   
            double elapsed = timer_get_time() - t0;
            LOGI("Downscale took %.3f ms", elapsed*1000.0);
            util_write_output(&image_scaled, "output/out_scaled.png");
        }

        Rect rects[256] = {};
        int num_rects = use_scaled_image ? process_image(&image_scaled, rects) : process_image(&image, rects);
        LOGI("Found %d rects", num_rects);

        if(use_scaled_image)
        {
            // correct rects positions / sizes
            const double scale = image.w > image.h ? image.w / (double)image_scaled.w : image.h / (double)image_scaled.h;
            for(int i = 0; i < num_rects; ++i)
            {
                Rect* r = &rects[i];
                r->x = (u16)round(r->x * scale);
                r->y = (u16)round(r->y * scale);
                r->w = (u16)round(r->w * scale);
                r->h = (u16)round(r->h * scale);

                for(int j = 0; j < 5; ++j)
                {
                    r->landmarks[j].x = (u16)round(r->landmarks[j].x * scale);
                    r->landmarks[j].y = (u16)round(r->landmarks[j].y * scale);
                }

                // add padding if needed
                const float rect_pad_pct = settings.box_padding_pct;

                float sw = (float)r->w * rect_pad_pct;
                float sh = (float)r->h * rect_pad_pct;

                r->x -= (u16)(sw/2.0);
                r->y -= (u16)(sw/2.0);
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

        for(int i = 0; i < settings.transform_count; ++i)
        {
            Transform* t = &settings.transforms[i];
            LOGI("Applying %s transform...", transform_type_to_str(t->type));
            transform_apply(&image, num_rects, rects,t->type);
        }

        if(settings.debug)
        {
            // draw debugging info on image
            for(int i = 0 ; i < num_rects; ++i)
            {
                transform_draw_rect(&image, rects[i],(Color){0,255,0,255}, false, 1.0);

                for(int l = 0; l < 5; ++l)
                {
                    PointU16 *lm = &rects[i].landmarks[l];
                    u16 x = MAX(0, (int)lm->x - 2);
                    u16 y = MAX(0, (int)lm->y - 2);

                    transform_draw_rect(&image, (Rect){x,y,4,4},(Color){255,0,255,255}, true, 1.0);
                }
            }
        }

        if(!settings.dry_run)
        {
            String outfile = StringFormat(scratch, "output/%s", settings.input_files[i].filename);
            LOGI("outfile %d: %.*s", i, outfile.len, outfile.data);
            util_write_output(&image, outfile.data);
        }
    }
    return 0;
}

int handle_video()
{

    Arena *arena_results = arena_create(ARENA_SIZE_MEDIUM);

    LOGI("Decoding video file %s", settings.input_file_text);
    
    Video vid = {};
    VideoCtx vid_ctx = {};

    // open
    bool opened = ffmpeg_open(settings.input_file_text, "output/out.mp4", &vid, &vid_ctx);
    if(!opened)
    {
        LOGE("Failed to open stream for video %s", settings.input_file_text);
        return 1;
    }

    for(;;)
    {
        double t0 = timer_get_time();

        // decode data
        bool decoded = ffmpeg_decode_ctx(&vid, &vid_ctx);
        if(!decoded)
        {
            LOGE("Failed to decode video %s", settings.input_file_text);
            break;
        }

        // run detections

        double elapsed = timer_get_time() - t0;
        LOGI("Decode took %.3f ms (frame count: %d / %ld), output: %p", elapsed*1000.0, vid.frame_count, vid.total_frame_count, vid.data);

        Image* images = (Image*)calloc(settings.thread_count, sizeof(Image));
        Image* images_scaled = (Image*)calloc(settings.thread_count, sizeof(Image));

        bool use_scaled[settings.thread_count] = {};

        u8 detect_buffers[settings.thread_count][0x9000] = {0};

        u32 output_count = 0;
        u8* output_ptrs[4096] = {};

        int frame_counter = 0;
        int actual_thread_count;

        const int skip_frames = 4;

        for(;;)
        {
            for(int i = 0; i < settings.thread_count; ++i)
            {
                memset(&images_scaled[i], 0, sizeof(Image));
                memset(detect_buffers[i], 0, 0x9000);
                use_scaled[i] = false;
            }

            if(frame_counter >= vid.frame_count)
                break;

            actual_thread_count = 0;

            // Create threads
            for(int i = 0; i < settings.thread_count; ++i)
            {
                if(frame_counter >= vid.frame_count)
                    break;

                Arena* arena = thread_arenas[actual_thread_count];
                pthread_t* thread = &threads[actual_thread_count];

                Image* image = &images[actual_thread_count];
                Image* image_scaled = &images_scaled[actual_thread_count];

                // reset arena
                arena_reset(arena);

                // fill out the image object
                image->detect_buffer = detect_buffers[actual_thread_count];
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
                    const float scaled_size = 640;
                    use_scaled[actual_thread_count] = transform_downscale(arena, image,image_scaled,scaled_size);
                    // util_write_output(image_scaled, "output/out_scaled.png");
                }

                reverse_rgb_order(use_scaled[i] ? image_scaled : image);

                // start detection thread
                if(pthread_create(thread, NULL, detect_faces, (void*)(use_scaled[i] ? image_scaled : image)) == 0)
                {
                    actual_thread_count++;
                }
                else
                {
                    LOGW("Failed to start thread");
                }

                frame_counter += (MIN(skip_frames, vid.frame_count - frame_counter)); // always want to evaluate final frame
            }
            
            // join all threads back
            for(int i = 0; i < actual_thread_count; ++i)
            {
                pthread_join(threads[i], NULL);
            }
            
            // Gather results
            int num_faces = 0;

            for(int i = 0; i < actual_thread_count; ++i)
            {
                Image* image = use_scaled[i] ? &images_scaled[i] : &images[i];

                num_faces = 0;

                if(image && image->result)
                {
                    u8* ret_rects = image->result;

                    int offset = 0;
                    int _faces_found = *((int*)(ret_rects));
                    offset += sizeof(int);

                    output_ptrs[image->frame_number] = (u8 *)arena_alloc(arena_results, sizeof(u32)+(_faces_found*sizeof(Rect)));

                    for(int j = 0; j < _faces_found; ++j)
                    {
                        Rect* r = (Rect*)(ret_rects+offset);
                        if(r->confidence < settings.confidence_threshold) // filter out low-confidence regions
                            continue;

                        if(use_scaled[i])
                        {
                            const double scale = vid.w > vid.h ? vid.w / (double)image->w : vid.h / (double)image->h;
                            r->x = (u16)round(r->x * scale);
                            r->y = (u16)round(r->y * scale);
                            r->w = (u16)round(r->w * scale);
                            r->h = (u16)round(r->h * scale);

                            for(int j = 0; j < 5; ++j)
                            {
                                r->landmarks[j].x = (u16)round(r->landmarks[j].x * scale);
                                r->landmarks[j].y = (u16)round(r->landmarks[j].y * scale);
                            }
                        }
                        
                        /* not sure if needed?
                        // make sure rectangles are within bounds of image
                        if(r->x >= image->w || r->y >= image->h) continue;

                        if(r->x + r->w > image->w) r->w = MAX(0, image->w - r->x - 1);
                        if(r->y + r->h > image->h) r->h = MAX(0, image->h - r->y - 1);
                        */

                        memcpy(output_ptrs[image->frame_number]+offset, r, sizeof(Rect)); offset += sizeof(Rect);
                        num_faces++;
                    }

                    memcpy(output_ptrs[image->frame_number], &num_faces, sizeof(u32));
                    output_count += num_faces;

                    reverse_rgb_order(image);
                }
            }

            LOGI("[Frame %d/%d]: num_faces: %d", frame_counter, vid.frame_count, num_faces);
        }
        
        // lerping

        RectList rl0 = {};
        RectList rl1 = {};

        //  
        //  |------|------|------|
        // rl0     x      x     rl1
        //
        
        for(int i = 0; i < vid.frame_count; ++i)
        {
            // Get Rects
            u8 *ptr = output_ptrs[i];

            if(ptr)
            {
                // rl0
                memcpy(&rl0, &rl1, sizeof(RectList));

                // rl1
                memcpy(&rl1.rect_count, ptr, sizeof(u32));
                rl1.rects = (Rect*)(ptr+sizeof(u32));
            }
            else
            {
                // gap frame
                // set rl0
                memcpy(&rl0, &rl1, sizeof(RectList));

                // look forward to find next valid frame to lerp to
                rl1.rect_count = 0;
                rl1.rects = NULL;

                int j = i;
                for(;;)
                {
                    j++;
                    if(j >= vid.frame_count)
                        break;

                    if(output_ptrs[j])
                    {
                        // valid frame (set rl1)
                        memcpy(&rl1.rect_count,output_ptrs[j], sizeof(u32));
                        rl1.rects = (Rect*)(output_ptrs[j]+sizeof(u32));
                        break;
                    }
                }

                int frames_in_between = j - i;

                if(frames_in_between == 0)
                {
                    // TODO
                    // there is no filled out frame ahead
                    // so just copy the results of rl0 forward
                    LOGW("TODO: No valid frames ahead! Copying forward");
                }

                if(rl0.rects && rl1.rects)
                {
                    for(int f = 0; f < frames_in_between; ++f)
                    {
                        // allocate space for frame

                        RectList* a = (rl0.rect_count >= rl1.rect_count) ? &rl0 : &rl1;
                        RectList* b = (rl0.rect_count >= rl1.rect_count) ? &rl1 : &rl0;

                        output_ptrs[i+f] = (u8 *)arena_alloc(arena_results, sizeof(u32)+(a->rect_count*sizeof(Rect)));
                        memcpy(output_ptrs[i+f], &a->rect_count, sizeof(u32));

                        int _offset = sizeof(u32);

                        int matched_count = 0;
                        int matches[256] = {};

                        // for each rect from a
                        for(int k = 0; k < a->rect_count; ++k)
                        {
                            Rect *ra = &a->rects[k];

                            // find best matching rect in b

                            float min_mv = FLT_MAX;
                            int min_index = -1;

                            for(int l = 0; l < b->rect_count; ++l)
                            {
                                bool is_matched = false;
                                for(int m = 0; m < matched_count; ++m)
                                {
                                    if(l == matches[m])
                                    {
                                        is_matched = true;
                                        break;
                                    }
                                }

                                if(is_matched)
                                    continue;

                                Rect *rb = &b->rects[l];

                                float dx = ABS(ra->x - rb->x);
                                float dy = ABS(ra->y - rb->y);
                                float dw = ABS(ra->w - rb->w);
                                float dh = ABS(ra->h - rb->h);

                                float mv = dx + dy + dw + dh;
                                if(mv < min_mv)
                                {
                                    min_mv = mv;
                                    min_index = l;
                                }
                            }

                            if(min_index >= 0)
                            {
                                // we found the best matching rect from b
                                // note the match
                                matched_count = 0;
                                matches[matched_count++] = k;

                                Rect *rb = &b->rects[min_index];

                                // gap rect
                                Rect *rg = (Rect*)((output_ptrs[i+f]+_offset));
                                _offset += sizeof(Rect);
                                
                                // lerp it!
                                float t = (f+1)/(float)(frames_in_between+1);

                                rg->x = (u16)lerp((float)ra->x, (float)rb->x, t);
                                rg->y = (u16)lerp((float)ra->y, (float)rb->y, t);
                                rg->w = (u16)lerp((float)ra->w, (float)rb->w, t);
                                rg->h = (u16)lerp((float)ra->h, (float)rb->h, t);
                                rg->confidence = (u16)lerp((float)ra->confidence, (float)rb->confidence, t);

                                for(int j = 0; j < 5; ++j)
                                {
                                    rg->landmarks[j].x = (u16)lerp((float)ra->landmarks[j].x, (float)rb->landmarks[j].x, t);
                                    rg->landmarks[j].y = (u16)lerp((float)ra->landmarks[j].y, (float)rb->landmarks[j].y, t);
                                }

                                // printf("  Lerping Rect %d (a: %u %u %u %u (%u), b: %u %u %u %u (%u), gap: %u %u %u %u (%u)\n", k, ra->x, ra->y, ra->w, ra->h, ra->confidence, rb->x, rb->y, rb->w, rb->h, rb->confidence, rg->x, rg->y, rg->w, rg->h, rg->confidence);
                            }
                        }

                        // copy any un-matched rects forward from a -> b
                        for(int k = 0; k < a->rect_count; ++k)
                        {
                            bool is_matched = false;
                            for(int l = 0; k < matched_count; ++k)
                            {
                                if(k == matches[l])
                                {
                                    is_matched = true;
                                    break;
                                }
                            }

                            if(is_matched)
                                continue;

                            // copy rect forward
                            Rect *rf = &a->rects[k];
                            Rect *rg = (Rect*)((output_ptrs[i+f]+_offset));
                            _offset += sizeof(Rect);

                            memcpy(rg, rf, sizeof(Rect));
                        }
                    }
                }
                else
                {
                    LOGW("One of the rect lists is null (%s, %s)", (rl0.rects == NULL ? "null" : "not null"), (rl1.rects == NULL ? "null" : "not null"));
                }

                i += MAX(0,(frames_in_between-1));
            }
        }


        // perform transformations

        LOGI("Applying transformation...");
        for(int i = 0; i < vid.frame_count; ++i)
        {
            // Get frame
            Image image = {};

            image.frame_number = i;
            image.data = &vid.data[(u64)i*vid.w*vid.h*3];
            image.w = vid.w;
            image.h = vid.h;
            image.n = 3;
            image.step = 3*image.w;
            
            // Get Rects
            u8 *ptr = output_ptrs[i];

            if(!ptr)
            {
                //LOGW("No output at Frame %d", i);
                continue;
            }

            u32 num_rects = 0;
            memcpy((u8*)&num_rects, ptr, sizeof(u32));
            ptr += sizeof(u32);
            Rect *rects = (Rect *)(ptr); 

            // add padding if needed
            const float rect_pad_pct = settings.box_padding_pct;
            for(int j = 0; j < num_rects; ++j)
            {
                float sw = (float)rects[j].w * rect_pad_pct;
                float sh = (float)rects[j].h * rect_pad_pct;

                rects[j].x -= (u16)(sw/2.0);
                rects[j].y -= (u16)(sw/2.0);
                rects[j].w += sw;
                rects[j].h += sh;

                if(rects[j].x < 0) rects[j].x = 0;
                if(rects[j].x >= image.w) rects[j].x = image.w-1;
                if(rects[j].y < 0) rects[j].y = 0;
                if(rects[j].y >= image.h) rects[j].y = image.h-1;
                if(rects[j].x+rects[j].w >=image.w) rects[j].w = (image.w-rects[j].x-1);
                if(rects[j].y+rects[j].h >=image.h) rects[j].h = (image.h-rects[j].y-1);
            }

            if(num_rects == 0)
            {
                printf("Zero rects at Frame %d!!\n", i);
            }
            else
            {
                //printf("[Frame %d][Rect %d] %u %u %u %u (%u)\n", i, 0, rects[0].x, rects[0].y, rects[0].w, rects[0].h, rects[0].confidence);
            }

            // Apply transformations
            for(int j = 0; j < settings.transform_count; ++j)
            {
                Transform* t = &settings.transforms[j];
                transform_apply(&image, num_rects, rects,t->type);
            }

            if(settings.debug)
            {
                // draw debugging info on image
                for(int j = 0 ; j < num_rects; ++j)
                {
                    transform_draw_rect(&image, rects[j],(Color){0,255,0,255}, false, 1.0);

                    for(int l = 0; l < 5; ++l)
                    {
                        PointU16 *lm = &rects[j].landmarks[l];
                        u16 x = MAX(0, (int)lm->x - 2);
                        u16 y = MAX(0, (int)lm->y - 2);
                        transform_draw_rect(&image, (Rect){x,y,4,4},(Color){255,0,255,255}, true, 1.0);
                    }
                }
            }
        }

        LOGI("Transformations done!");

        if(!settings.dry_run)
        {
            // encode data
            double _t0 = timer_get_time();
            bool encoded = ffmpeg_encode_ctx(&vid, &vid_ctx);
            if(!encoded)
            {
                LOGE("Failed to write output file");
                return 1;
            }

            double _elapsed = timer_get_time() - _t0;
            LOGI("Encode took %.3f ms", _elapsed*1000.0);
        }

        // exit if video is done
        if(vid.decode_complete)
        {
            if(!settings.dry_run) ffmpeg_encode_done(&vid_ctx);
            LOGI("Complete!");
            break;
        }
    }

    // ffmpeg_close(&vid_ctx);

    return 0;
}

bool init(int argc, char **args)
{
    // init
    timer_init();
    log_init(0);

    time_t t;
    srand((unsigned) time(&t));

    // set default settings
    memset(settings.input_file_text,0,256);
    settings.thread_count = MAX(1, util_get_core_count()); // default to num_cores
    settings.asset_type = TYPE_IMAGE;
    settings.classification = CLASS_FACE;
    settings.transform_count = 0;
    settings.debug = false;
    settings.confidence_threshold = 30;
    settings.nms_iou_threshold = 0.6;
    settings.blur_strength = 0.50;
    settings.has_texture = false;
    settings.no_scale = false;
    settings.block_scale = 0.16;
    settings.input_file_count = 0;
    settings.max_buffer_size = 4UL*1024UL*1024UL*1024UL; // 4GB
    settings.box_padding_pct = 0.15;
    settings.dry_run = false;

    bool parse = parse_args(&settings, argc, args);
    if(!parse) return false;

    // print settings
    LOGI("--- Settings ---");
    LOGI("  Thread Count: %d", settings.thread_count);
    LOGI("  Confidence Threshold: %d", settings.confidence_threshold);
    LOGI("  NMS IOU Threshold: %f", settings.nms_iou_threshold);
    LOGI("  Texture: %s", settings.has_texture ? settings.texture_image_path : "(None)");
    LOGI("  Block Scale: %f", settings.block_scale);
    LOGI("  Max Buffer Size: %lu B", settings.max_buffer_size);
    LOGI("  Box Padding Percentage: %f", settings.box_padding_pct);
    LOGI("  Dry Run: %s", BOOLSTR(settings.dry_run));
    LOGI("  Debug: %s", settings.debug ? "ON" : "OFF");
    LOGI("----------------");
    
    // initialize memory arenas used in program
    for(int i = 0; i < settings.thread_count; ++i)
    {
        thread_arenas[i] = arena_create(ARENA_SIZE_LARGE);
    }
    scratch = arena_create(ARENA_SIZE_MEDIUM);

    // initialize model data
    detect_init();

    return true;
}

void print_help()
{
    printf("\n[USAGE]\n");
    printf("  censorman <in_file> -o <out_file> -d {class_list} -t {transform_list} [-c confidence_threshold][-j thread_count] [--debug] [--image <texture_image_path>] [--block_scale <block_scale>] [--blur_strngth <blur_strength>] [--buffer_size <buffer_size>] [--dry_run] [--is_quiet]\n");
    printf("\n[DESCRIPTION]\n  Takes an image file, detects regions of human faces (for now), applies transformations on those regions and writes back an output image file\n");
    printf("\n[ARGUMENTS]\n");
    printf("  in_file:              Path to input image file (or folder) (.jpg, .png, .bmp)\n");
    printf("  out_file:             Path to output image file (.jpg, .png, .bmp)\n");
    printf("  class_list:           {face}\n");
    printf("  transform_list:       {pixelate, blur, blackout, scramble, texture}\n");
    printf("  confidence_threshold: Discard any boxes lower than this (0 - 100)\n");
    printf("  thread_count:         How many threads to use to detect (default to number of cores)\n");
    printf("  debug:                Print debug info and draw boxes on output image\n");
    printf("  texture_image_path:   Used with 'texture' transform\n");
    printf("  block_scale:          Value between 0.0 and 1.0. Used to scale blocks in pixelate transform\n");
    printf("  blur_strength:        Value between 0.0 and 1.0. Used in the Gaussian Blur (Default: 0.50)\n");
    printf("  buffer_size:          Number of bytes for video frames during conversion (Default: 4 GB)\n");
    printf("  box_padding_pct:      Added percentage of padding to detected boxes (Default: 0.15)");
    printf("  dry_run:              Prevents writing output image or video file\n");
    printf("  is_quiet:             Suppress standard log output\n");
    printf("\n");
}

bool parse_args(ProgramSettings* settings, int argc, char* argv[])
{
    if(argc <= 1)
    {
        print_help();
        return false;
    }

    bool input_file_needed = true;

    for(int i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            switch(argv[i][1])
            {
                case '-':
                {
                    if(STR_EQUAL(&argv[i][2],"debug"))
                        settings->debug = true;
                    else if(STR_EQUAL(&argv[i][2],"quiet"))
                        is_quiet = true;
                    else if(STR_EQUAL(&argv[i][2],"dry_run"))
                        settings->dry_run = true;
                    else if(STR_EQUAL(&argv[i][2],"no_scale"))
                        settings->no_scale = true;
                    else if(STR_EQUAL(&argv[i][2],"block_scale"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            float f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->block_scale = f;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"blur_strength"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            float f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->blur_strength = f;
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
                    else if(STR_EQUAL(&argv[i][2],"buffer_size"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            u64 n = atol(argv[i]);
                            if(n > 0) settings->max_buffer_size = n;
                        }
                    }
                    else if(STR_EQUAL(&argv[i][2],"box_padding_pct"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            float f = atof(argv[i]);
                            CLAMP(f, 0.0, 1.0);
                            settings->box_padding_pct = f;
                        }
                    }
                }   break;
                case 'o':
                    break;
                case 'd':
                    break;
                case 'c':
                {
                    int n = atoi(argv[i+1]);
                    settings->confidence_threshold = n == 0 ? settings->confidence_threshold : n;
                }   break;
                case 't':
                {
                    if(i < argc-1)
                    {
                        // parse transforms
                        char* p = argv[i+1];
                        int len = strlen(p);
                        char buf[256] = {0};
                        int bufi = 0;
                        bool process = false;

                        for(int i = 0; i < len; ++i)
                        {
                            int c = *p++;
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

                                memset(buf,256,0);
                                bufi = 0;

                                if(type != TRANSFORM_TYPE_NONE)
                                {
                                    Transform *t = &settings->transforms[settings->transform_count++];
                                    t->type = type;
                                }
                            }
                        }
                    }
                } break;
                case 'j': {
                    if(i < argc-1)
                    {
                        int n = atoi(argv[i+1]);
                        settings->thread_count = n == 0 ? settings->thread_count : n;
                    }
                }   break;
                default:
                    break;
            }
        }
        else if(input_file_needed)
        {
            // assume input file
            strncpy(settings->input_file_text, argv[i], 255);
            input_file_needed = false;
        }
    }

    return true;
}
