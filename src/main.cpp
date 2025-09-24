#include <stdio.h>

#include "base.h"
#include "platform.h"
#include "detect.h"
#include "ffmpeg.h"
#include "transform.h"
#include "util.h"

#define CENSORMAN_VERSION 1

// TODO
// [ ] Get Windows support working
// [ ] Add a output_size CLI parameter
// [ ] Optimize memory usage for --no_encoding
// [ ] Add Return Code Enum for any errors and success
// [ ] Add padding to sub-images
// [ ] Thread the transformations
// [ ] Consider enabling crude first pass to address video discontinuity
// [ ] Add lots of test images and --tester mode
// [ ] Implement thread pool (mutex vs spin-lock)
// [ ] Statically compile ncnn (Tencent) inference engine
// [ ] Add audio stream encoding (1:1)
// [ ] Add YuNet model (.bin and .param) to compare accuracy
// [ ] Fix builds for MacOS


// DONE
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
// [x] Lerping rects in video
// [x] Add scramble transform
// [x] Add blur transform
// [x] Add image scaling function
// [x] Open a video file and read image frames
// [x] Write output video file

Arena* scratch = {};
Arena* thread_arenas[MAX_ARENAS] = {};

Timer timer = {};
ProgramSettings settings = {};
Image texture_image = {};
double begin_time = 0.0;

bool init(int argc, char **args);
bool parse_args(ProgramSettings* settings, int argc, char* argv[]);
int process_image(Image* image,Rect* ret_rects);
int handle_image();
int handle_video();
void draw_debugging_info(Image* image, Rect* rects, int num_rects);

int main(int argc, char** args)
{
    bool initialized = init(argc, args);
    if(!initialized)
        return 1;

    // check input
    char ext[10] = {};
    int ext_len = str_get_extension(settings.input_file_text, ext, 10);
    if(ext_len == 0)
    {
        // load up images from folder
        LOGV("Loading image from folder %s", settings.input_file_text);
        strncpy(settings.input_directory,settings.input_file_text, strlen(settings.input_file_text));

        String ext1 = S(".png");
        String ext2 = S(".jpg");
        String ext3 = S(".bmp");

        String exts[] = {ext1, ext2, ext3};
        
        String* files;
        int count = platform_get_files_in_folder(scratch, str_from_cstr(settings.input_directory), exts, 3, &files);

        for (int i = 0; i < count; ++i)
        {
            LOGV("File %d: %.*s", i + 1, files[i].len, files[i].data);
            strncpy(settings.input_files[i].filename, files[i].data, files[i].len);
        }
        settings.input_file_count = count;

        arena_reset(scratch);
    }
    else
    {
        // single input file, not a folder

        LOGV("File extension: %s", ext);
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
    thread_init(settings.thread_count);

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

        LOGV("infile: %.*s", infile.len, infile.data);

        bool loaded = util_load_image(infile.data, &image);
        if(!loaded) return 1;

        FILE *bbx_file = NULL;

        if(settings.has_bbx_output)
        {
            // open bbx file for output
            bbx_file = fopen(settings.bbx_output,"wb");
            if(!bbx_file)
            {
                LOGW("Failed to open Bounding Boxes file for writing %s", settings.bbx_output);
            }
            else
            {
                // write file header
                FileWriteStr(bbx_file, "BBX");
                FileWriteU8(bbx_file,  BBX_VERSION);
                FileWriteU16(bbx_file, (u16)image.w);
                FileWriteU16(bbx_file, (u16)image.h);
                FileWriteF32(bbx_file, 0.0);
                FileWriteU32(bbx_file, 1); // only one frame for an image
            }
        }

        Image image_scaled = {};
        int scaled_size = settings.scaled_size_image;
        bool use_scaled_image = false;

        if(!settings.no_scale)
        {
            stopwatch_start();
            use_scaled_image = transform_downscale(NULL, &image,&image_scaled,scaled_size,0); // @TODO: rotation
            LOGI("Downscale took %.3f ms", stopwatch_time()*1000.0);
            util_write_output(&image_scaled, "output/out_scaled.png");
        }

        Rect rects[256] = {};
        int num_rects = use_scaled_image ? process_image(&image_scaled, rects) : process_image(&image, rects);
        LOGV("Found %d rects", num_rects);

        if(use_scaled_image)
        {
            // correct rects positions / sizes
            const double scale = image.w > image.h ? image.w / (double)image_scaled.w : image.h / (double)image_scaled.h;
            for(int i = 0; i < num_rects; ++i)
            {
                Rect* r = &rects[i];
                r->x = (int)round(r->x * scale);
                r->y = (int)round(r->y * scale);
                r->w = (int)round(r->w * scale);
                r->h = (int)round(r->h * scale);

                for(int j = 0; j < 5; ++j)
                {
                    r->landmarks[j].x = (int)round(r->landmarks[j].x * scale);
                    r->landmarks[j].y = (int)round(r->landmarks[j].y * scale);
                }

                // add padding if needed
                const float rect_pad_pct = settings.box_padding_pct;

                float sw = (float)r->w * rect_pad_pct;
                float sh = (float)r->h * rect_pad_pct;

                r->x -= (int)(sw/2.0);
                r->y -= (int)(sw/2.0);
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


        if(bbx_file)
        {
            FileWriteU32(bbx_file, 0); // frame index
            FileWriteU16(bbx_file, num_rects);

            for(int i  = 0; i < num_rects; ++i)
            {
                util_write_bbx_to_file(bbx_file, &rects[i]);
            }
        }

        for(int i = 0; i < settings.transform_count; ++i)
        {
            Transform* t = &settings.transforms[i];
            LOGV("Applying %s transform...", transform_type_to_str(t->type));
            transform_apply(&image, num_rects, rects,t->type);
        }

        if(settings.debug)
        {
            draw_debugging_info(&image, rects, num_rects);
        }

        if(!settings.no_encoding)
        {
            String outfile = StringFormat(scratch, "output/%s", settings.input_files[i].filename);
            LOGI("outfile %d: %.*s", i, outfile.len, outfile.data);
            util_write_output(&image, outfile.data);
        }

        if(bbx_file)
        {
            fclose(bbx_file);
        }

    }
    return 0;
}

int handle_video()
{
    Arena *arena_results = arena_create(ARENA_SIZE_MEDIUM);

    LOGI("Opening video file '%s'...", settings.input_file_text);
    
    Video vid = {};
    VideoCtx vid_ctx = {};

    stopwatch_start();

    // open
    bool opened = ffmpeg_open(settings.input_file_text, "output/out.mp4", &vid, &vid_ctx);
    if(!opened)
    {
        LOGE("Failed to open stream for video %s", settings.input_file_text);
        return 1;
    }

    LOGI("Opened. [%6.3f s]", stopwatch_time());

    FILE *bbx_file = NULL;
    if(settings.has_bbx_output)
    {
        // open bbx file for output
        bbx_file = fopen(settings.bbx_output,"wb");
        if(!bbx_file)
        {
            LOGW("Failed to open Bounding Boxes file for writing %s", settings.bbx_output);
        }
        else
        {
            // write file header
            FileWriteStr(bbx_file, "BBX");
            FileWriteU8(bbx_file,  BBX_VERSION);
            FileWriteU16(bbx_file, (u16)vid.w);
            FileWriteU16(bbx_file, (u16)vid.h);
            FileWriteF32(bbx_file, (f32)vid.fps);
            FileWriteU32(bbx_file, 0x00000000); // stub for frame count
        }
    }

    double total_time_decoding = 0.0;
    double total_time_detecting = 0.0;
    double total_time_transforming = 0.0;
    double total_time_encoding = 0.0;

    u32 total_frames_processed = 0;

    for(;;)
    {
        LOGI("Decoding Chunk...");
        stopwatch_start();

        // decode data
        bool decoded = ffmpeg_decode_ctx(&vid, &vid_ctx);
        if(!decoded)
        {
            LOGE("Failed to decode video %s", settings.input_file_text);
            break;
        }

        double decode_time = stopwatch_time();
        total_time_decoding += decode_time;
        LOGI("Decoded Chunk (Size: %d). [%6.3f s] ", vid.frame_count, decode_time);

        // If no frames were decoded, we are done
        if(vid.frame_count == 0 && vid.decode_complete)
        {
            break;
        }

        // run detections

        LOGI("Detecting faces...");
        stopwatch_start();

        Image* images = (Image*)calloc(settings.thread_count, sizeof(Image));
        Image* images_scaled = (Image*)calloc(settings.thread_count, sizeof(Image));

        bool use_scaled[settings.thread_count] = {};

        u8 detect_buffers[settings.thread_count][0x9000] = {};

        u32 output_count = 0;
        u8* output_ptrs[4096] = {};

        int frame_counter = 0;
        int actual_thread_count;

        int skip_frames = MAX(1, (int)(settings.frame_smoothing_window * vid.fps));
        LOGV("Number of skip frames: %d (Based on %f smoothing window)", skip_frames, settings.frame_smoothing_window);

        for(;;)
        {
            for(int i = 0; i < settings.thread_count; ++i)
            {
                memset(&images_scaled[i], 0, sizeof(Image));
                memset(detect_buffers[i], 0, 0x9000);
                use_scaled[i] = false;
            }

            if(frame_counter >= vid.frame_count-1)
                break;

            actual_thread_count = 0;

            // Create threads
            for(int i = 0; i < settings.thread_count; ++i)
            {
                Arena* arena = thread_arenas[actual_thread_count];
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
                    int scaled_size = settings.scaled_size_video;
                    use_scaled[actual_thread_count] = transform_downscale(arena, image,image_scaled,scaled_size, vid.rotation);
#if 1
                    char outfile[256] = {};
                    snprintf(outfile, 255, "output/debug/%d.png",frame_counter);
                    util_write_output(image_scaled, outfile);
#endif
                }

                reverse_rgb_order(use_scaled[i] ? image_scaled : image);

                // start detection thread
                if(thread_create(&threads[actual_thread_count], detect_faces, (void*)(use_scaled[i] ? image_scaled : image)) == 0)
                {
                    actual_thread_count++;
                }
                else
                {
                    LOGW("Failed to start thread");
                }


                int frame_advance = MIN(skip_frames, vid.frame_count - frame_counter - 1);
                if(frame_advance <= 0)
                    break;

                frame_counter += frame_advance; // always want to evaluate final frame
            }
            
            // join all threads back
            for(int i = 0; i < actual_thread_count; ++i)
            {
                thread_join(threads[i]);
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

                        if(use_scaled[i])
                        {
                            const float scale = vid.w > vid.h ? vid.w / (float)image->w : vid.h / (float)image->h;
                            transform_rect_upscale_rotate_inverse(r, image->w, image->h, vid.w, vid.h, image->rotation);
                        }

                        memcpy(output_ptrs[image->frame_number]+offset, r, sizeof(Rect)); offset += sizeof(Rect);
                        num_faces++;
                    }

                    memcpy(output_ptrs[image->frame_number], &num_faces, sizeof(u32));
                    output_count += num_faces;

                    reverse_rgb_order(image);
                }
            }

            LOGV("[Frame %d/%d]: num_faces: %d", frame_counter+1, vid.frame_count, num_faces);
        }

        double detect_time = stopwatch_time();
        total_time_detecting += detect_time;
        LOGI("Detection done. [%6.3f s]", detect_time);
        
        //  
        //  |------|------|------|
        // rl0     x      x     rl1
        //

        RectList rl0 = {};
        RectList rl1 = {};

        for (int i = 0; i < vid.frame_count; )
        {
            u8 *ptr = output_ptrs[i];

            // rl0 <- previous frame
            memcpy(&rl0, &rl1, sizeof(RectList));

            if (ptr)
            {
                // valid frame, rl1 <- current frame
                memcpy(&rl1.rect_count, ptr, sizeof(u32));
                rl1.rects = (Rect*)(ptr + sizeof(u32));

                i++; // advance to next frame
                continue;
            }
            else
            {
                // Gap frame — look ahead to find next valid frame
                int j = i + 1;
                while (j < vid.frame_count && !output_ptrs[j])
                    j++;

                int frames_in_between = j - i;

                // If no future valid frame, copy rl0 forward for remaining frames
                if (j >= vid.frame_count)
                {
                    for (int f = 0; f < frames_in_between; ++f)
                    {
                        // allocate space for output_ptrs
                        int rc = rl0.rect_count;
                        output_ptrs[i+f] = (u8*)arena_alloc(arena_results, sizeof(u32) + rc*sizeof(Rect));
                        memcpy(output_ptrs[i+f], &rc, sizeof(u32));
                        memcpy(output_ptrs[i+f] + sizeof(u32), rl0.rects, rc*sizeof(Rect));
                    }
                    break; // reached end of video
                }

                // Set rl1 to the next valid frame
                memcpy(&rl1.rect_count, output_ptrs[j], sizeof(u32));
                rl1.rects = (Rect*)(output_ptrs[j] + sizeof(u32));

                // Now interpolate frames between rl0 and rl1
                for (int f = 0; f < frames_in_between; ++f)
                {
                    // Decide which rect list is bigger
                    RectList *a = (rl0.rect_count >= rl1.rect_count) ? &rl0 : &rl1;
                    RectList *b = (rl0.rect_count >= rl1.rect_count) ? &rl1 : &rl0;

                    int rc = a->rect_count;
                    u8 *out = (u8*)arena_alloc(arena_results, sizeof(u32) + rc*sizeof(Rect));
                    memcpy(out, &rc, sizeof(u32));

                    int offset = sizeof(u32);

                    int matched_count = 0;
                    int matches[256] = {};

                    // Exponential smoothing / lerp for matched rects
                    for (int k = 0; k < a->rect_count; ++k)
                    {
                        float min_mv = FLT_MAX;
                        int min_index = -1;

                        for (int l = 0; l < b->rect_count; ++l)
                        {
                            bool already_matched = false;
                            for (int m = 0; m < matched_count; ++m)
                                if (matches[m] == l) { already_matched = true; break; }
                            if (already_matched) continue;

                            Rect *ra = &a->rects[k];
                            Rect *rb = &b->rects[l];

                            float dx = ABS(ra->x - rb->x);
                            float dy = ABS(ra->y - rb->y);
                            float dw = ABS(ra->w - rb->w);
                            float dh = ABS(ra->h - rb->h);

                            float mv = dx + dy + dw + dh;
                            if (mv < min_mv)
                            {
                                min_mv = mv;
                                min_index = l;
                            }
                        }

                        Rect *rg = (Rect*)(out + offset);
                        offset += sizeof(Rect);

                        if (min_index >= 0)
                        {
                            // mark as matched
                            matches[matched_count++] = min_index;

                            Rect *ra = &a->rects[k];
                            Rect *rb = &b->rects[min_index];

                            const float alpha = 0.2f; // smoothing
                            rg->x = (int)exponential_smooth((float)ra->x, (float)rb->x, alpha, f);
                            rg->y = (int)exponential_smooth((float)ra->y, (float)rb->y, alpha, f);
                            rg->w = (int)exponential_smooth((float)ra->w, (float)rb->w, alpha, f);
                            rg->h = (int)exponential_smooth((float)ra->h, (float)rb->h, alpha, f);
                            rg->confidence = (int)exponential_smooth((float)ra->confidence, (float)rb->confidence, alpha, f);

                            for (int j2 = 0; j2 < 5; ++j2)
                            {
                                rg->landmarks[j2].x = (int)exponential_smooth((float)ra->landmarks[j2].x, (float)rb->landmarks[j2].x, alpha, f);
                                rg->landmarks[j2].y = (int)exponential_smooth((float)ra->landmarks[j2].y, (float)rb->landmarks[j2].y, alpha, f);
                            }
                        }
                        else
                        {
                            // No match, just copy forward from a
                            memcpy(rg, &a->rects[k], sizeof(Rect));
                        }
                    }

                    // Copy output
                    output_ptrs[i+f] = out;
                }

                // Advance i past interpolated frames
                i += frames_in_between;
            }
        }

        int zero_rects_frames = 0;

        // perform transformations
        LOGI("Applying transformations...");
        stopwatch_start();

        LOGV("Iterating through video frames (Total: %d)", vid.frame_count);
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
                LOGV("No output at Frame %d", i);
                continue;
            }

            u32 num_rects = 0;
            memcpy((u8*)&num_rects, ptr, sizeof(u32));
            ptr += sizeof(u32);
            Rect *rects = (Rect *)(ptr); 

            if(bbx_file)
            {
                FileWriteU32(bbx_file, total_frames_processed+i); // frame index
                FileWriteU16(bbx_file, num_rects);
            }

            // add padding if needed
            const float rect_pad_pct = settings.box_padding_pct;

            for(int j = 0; j < num_rects; ++j)
            {
                float sw = (float)rects[j].w * rect_pad_pct;
                float sh = (float)rects[j].h * rect_pad_pct;

                rects[j].x -= (int)(sw/2.0);
                rects[j].y -= (int)(sw/2.0);
                rects[j].w += sw;
                rects[j].h += sh;

                bool no_rotate = (settings.no_rotate && (vid.rotation == 90 || vid.rotation == 270));
                int w = no_rotate ? image.h : image.w;
                int h = no_rotate ? image.w : image.h;

                if(rects[j].x < 0) rects[j].x = 0;
                if(rects[j].x >= w) rects[j].x = w-1;
                if(rects[j].y < 0) rects[j].y = 0;
                if(rects[j].y >= h) rects[j].y = h-1;
                if(rects[j].x+rects[j].w >= w) rects[j].w = (w-rects[j].x-1);
                if(rects[j].y+rects[j].h >= h) rects[j].h = (h-rects[j].y-1);

                if(bbx_file)
                {
#if 0
                    if(settings.no_rotate)
                    {
                        if(vid.rotation == 90)
                        {
                            int ox = rects[j].y;
                            int oy = (image.h - 1) - rects[j].x;

                            rects[j].x = (u16)ox;
                            rects[j].y = (u16)oy;

                            SWAP(u16,rects[j].w, rects[j].h);

                            for(int k = 0; k < 5; ++k)
                            {
                                int ox2 = rects[j].landmarks[k].y;
                                int oy2 = (image.h - 1) - rects[j].landmarks[k].x;

                                rects[j].landmarks[k].x = (u16)ox2;
                                rects[j].landmarks[k].y = (u16)oy2;
                            }
                        }
                        else if(vid.rotation == 270)
                        {
                            int ox = (image.w - 1) - rects[j].y;
                            int oy = rects[j].x;

                            rects[j].x = (u16)ox;
                            rects[j].y = (u16)oy;

                            SWAP(u16,rects[j].w, rects[j].h);

                            for(int k = 0; k < 5; ++k)
                            {
                                int ox2 = (image.w - 1) - rects[j].landmarks[k].y;
                                int oy2 = rects[j].landmarks[k].x;

                                rects[j].landmarks[k].x = (u16)ox2;
                                rects[j].landmarks[k].y = (u16)oy2;
                            }

                        }
                    }
#endif
                    if(i == 5000)
                    {
                        LOGW("DEBUG: Pos: [%u,%u] Size: [%u,%u] Confidence: %u", rects[j].x, rects[j].y, rects[j].w, rects[j].h, rects[j].confidence);
                    }
                    util_write_bbx_to_file(bbx_file, &rects[j]);
                }

                LOGV("[Frame %d][Rect %d] %u %u %u %u (%u%)", i, j, rects[j].x, rects[j].y, rects[j].w, rects[j].h, rects[j].confidence);
            }

            if(num_rects == 0)
            {
                zero_rects_frames++;
            }

            // Apply transformations
            for(int j = 0; j < settings.transform_count; ++j)
            {
                Transform* t = &settings.transforms[j];
                transform_apply(&image, num_rects, rects,t->type);
            }

            if(settings.debug)
            {
                draw_debugging_info(&image, rects, num_rects);
            }
        }

        LOGV("Number of zero rect frames: %d", zero_rects_frames);

        double transform_time = stopwatch_time();
        total_time_transforming += transform_time;
        LOGI("Transformations done. [%6.3f s]", transform_time);

        total_frames_processed += vid.frame_count;
        if(bbx_file)
        {
            fflush(bbx_file);
        }

        if(!settings.no_encoding)
        {
            LOGI("Encoding chunk...");
            stopwatch_start();
            bool encoded = ffmpeg_encode_ctx(&vid, &vid_ctx);
            if(!encoded)
            {
                LOGE("Failed to write output file");
                return 1;
            }

            double encode_time = stopwatch_time();
            total_time_encoding += encode_time;
            LOGI("Encode done. [%6.3f s]", encode_time);
        }

        LOGI("Progress: %d / %d [%d%]", total_frames_processed, vid.total_frame_count, (int)(100.0f*total_frames_processed / (float)vid.total_frame_count));

        // exit if video is done
        if(vid.decode_complete)
        {
            if(!settings.no_encoding) ffmpeg_encode_done(&vid_ctx);
            break;
        }

    }

    if(bbx_file)
    {
        // Write the total frame count in the header after done
        FileWriteU32AtIndex(bbx_file, total_frames_processed, BBX_FRAME_COUNT_OFFSET);
        fclose(bbx_file);
    }

    double total_processing_time = total_time_decoding + total_time_detecting + total_time_transforming + total_time_encoding;
    double total_elapsed_time = timer_get_time() - begin_time;

    LOGI("Complete!");

    LOGI("Total Time Decoding:     %6.3f s [%02d%]", total_time_decoding, (int)(100.0f*total_time_decoding / total_processing_time));
    LOGI("Total Time Detecting:    %6.3f s [%02d%]", total_time_detecting, (int)(100.0f*total_time_detecting / total_processing_time));
    LOGI("Total Time Transforming: %6.3f s [%02d%]", total_time_transforming, (int)(100.0f*total_time_transforming / total_processing_time));
    LOGI("Total Time Encoding:     %6.3f s [%02d%]", total_time_encoding, (int)(100.0f*total_time_encoding / total_processing_time));
    LOGI("Total Time:              %6.3f s", total_elapsed_time);


    // ffmpeg_close(&vid_ctx);

    return 0;
}

void draw_debugging_info(Image* image, Rect* rects, int num_rects)
{
    Color color_list[] = {
        {255,0,0,255},
        {0,255,0,255},
        {0,0,255,255},
        {255,255,0,255},
        {255,0,255,255}
    };

    Color color_bad  = {255,0,0,255};
    Color color_good = {0,255,0,255};

    for(int j = num_rects - 1; j >= 0; --j)
    {
        Color color = transform_blend_color(color_bad, color_good, (rects[j].confidence / 100.0f));

        transform_draw_rect(image, rects[j],color, false, 1.0);
        transform_draw_string(image, rects[j].x+1, rects[j].y+1, color,"%u", rects[j].confidence);

        for(int l = 0; l < 5; ++l)
        {
            Point *lm = &rects[j].landmarks[l];
            transform_draw_circle(image, lm->x, lm->y, 2, color_list[l], true, 1.0);
        }
    }
}

bool init(int argc, char **args)
{
    // init
    timer_init();
    begin_time = timer_get_time();

    log_init(0);

    time_t t;
    srand((unsigned) time(&t));

    // set default settings
    memset(settings.input_file_text,0,256);
    memset(settings.bbx_output,0,256);
    settings.thread_count = MAX(1, util_get_core_count()*2); // default to num_cores
    settings.asset_type = TYPE_IMAGE;
    settings.classification = CLASS_FACE;
    settings.transform_count = 0;
    settings.debug = false;
    settings.confidence_threshold = 20;
    settings.nms_iou_threshold = 0.6;
    settings.blur_strength = 0.50;
    settings.has_texture = false;
    settings.no_rotate = false;
    settings.no_scale = false;
    settings.block_scale = 0.16;
    settings.frame_smoothing_window = 0.150;
    settings.input_file_count = 0;
    settings.max_buffer_size = 1UL*1024UL*1024UL*1024UL; // 1GB
    settings.box_padding_pct = 0.15;
    settings.no_encoding = false;
    settings.verbose = false;
    settings.has_bbx_output = false;
    settings.scaled_size_image = 640;
    settings.scaled_size_video = 320;

    bool parse = parse_args(&settings, argc, args);
    if(!parse) return false;

    if(settings.verbose)
    {
        log_level = LOG_TYPE_VERBOSE;
    }

    // print title
    if(!is_quiet)
    {
        printf("[CENSORMAN V%d]\n", CENSORMAN_VERSION);
        printf("    _O_\n");
        printf("  /|-x-|\\\n");
        printf(" /  \\_/  \\\n");
        printf("    / \\\n");
        printf("  _/   \\_\n");
    }

    // print settings
    LOGI("");
    LOGI("=============== Settings ===============");
    LOGI("  File Count:             %d", settings.input_file_count);
    LOGI("  Asset Type:             %s", settings.asset_type == TYPE_IMAGE ? "Image" : "Video");
    LOGI("  Thread Count:           %d", settings.thread_count);
    LOGI("  Confidence Threshold:   %d", settings.confidence_threshold);
    LOGI("  Blur Strength:          %f", settings.blur_strength);
    LOGI("  NMS IOU Threshold:      %f", settings.nms_iou_threshold);
    LOGI("  Block Scale:            %f", settings.block_scale);
    LOGI("  Texture:                %s", settings.has_texture ? settings.texture_image_path : "(None)");
    LOGI("  Max Buffer Size:        %lu B", settings.max_buffer_size);
    LOGI("  Box Padding Percent:    %f", settings.box_padding_pct);
    LOGI("  Frame Smoothing Window: %f", settings.frame_smoothing_window);
    LOGI("  No Encoding:            %s", BOOLSTR(settings.no_encoding));
    LOGI("  Downscaling:            %s (%d px)", settings.no_scale ? "No" : "Yes", settings.asset_type == TYPE_IMAGE ? settings.scaled_size_image : settings.scaled_size_video);
    LOGI("  No Rotate:              %s", BOOLSTR(settings.no_rotate));
    LOGI("  Bounding Box Output:    %s", settings.has_bbx_output ? settings.bbx_output : "(None)");
    LOGI("  Debug:                  %s", settings.debug ? "ON" : "OFF");
    LOGI("  Verbose:                %s", settings.verbose ? "ON" : "OFF");
    LOGI("========================================");
    LOGI("");

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
                    else if(STR_EQUAL(&argv[i][2],"verbose"))
                        settings->verbose = true;
                    else if(STR_EQUAL(&argv[i][2],"no_encoding"))
                        settings->no_encoding = true;
                    else if(STR_EQUAL(&argv[i][2],"no_scale"))
                        settings->no_scale = true;
                    else if(STR_EQUAL(&argv[i][2],"no_rotate"))
                        settings->no_rotate = true;
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
                    else if(STR_EQUAL(&argv[i][2],"frame_smoothing_window"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            float f = atof(argv[i]);
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
                            strncpy(settings->bbx_output, argv[i], 255);
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
                        char buf[256] = {};
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

                                memset(buf,0,256);
                                bufi = 0;

                                if(type != TRANSFORM_TYPE_NONE)
                                {
                                    Transform *t = &settings->transforms[settings->transform_count++];
                                    t->type = type;
                                }
                                else
                                {
                                    LOGW("Transform type is None (string: %s)", buf);
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
