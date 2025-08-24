#include <stdio.h>
#include <pthread.h>

#include "base.h"
#include "platform.h"
#include "detect.h"
//#include "ffmpeg.h"
#include "transform.h"
#include "util.h"

// TODO
//
// [x] Add scramble transform
// [ ] Add padding to sub-images
// [ ] Add blur transform
// [x] Add image scaling function
// [ ] Add lots of test images and --tester mode
// [ ] Implement thread pool (spin-lock)
// [ ] Open a video file and read image frames
// [ ] Write output video file

Arena* scratch = {0};
Arena* arenas[MAX_ARENAS] = {0};
Timer timer = {0};
ProgramSettings settings = {};
pthread_t *threads = NULL;
Image texture_image = {};

bool init(int argc, char **args);
bool parse_args(ProgramSettings* settings, int argc, char* argv[]);
int process_image(Image* image,Rect* ret_rects);

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

        bool is_video = (STR_EQUAL(ext, ".mp4") || STR_EQUAL(ext, ".mov") || STR_EQUAL(ext, ".MP4") || STR_EQUAL(ext, ".MOV"));
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

    if(settings.asset_type == TYPE_VIDEO)
    {
        Video vid = {};
        double t0 = timer_get_time();
        bool decode_result = true; //ffmpeg_decode("assets/vid1.mp4", &vid);
        double elapsed = timer_get_time() - t0;
        LOGI("Decode took %.3f ms", elapsed*1000.0);
    }

    Image image = {};

    for(int i = 0; i < settings.input_file_count; ++i)
    {
        String infile;
        infile = StringFormat(scratch, "%s/%s", settings.input_directory, settings.input_files[i].filename);

        LOGI("infile: %.*s", infile.len, infile.data);

        bool loaded = util_load_image(infile.data, &image);
        if(!loaded) return 1;

        Image image_scaled = {};
        const int scaled_size = 480;
        bool use_scaled_image = false;

        if(!settings.no_scale)
        {
            double t0 = timer_get_time();
            use_scaled_image = transform_downscale_image(NULL, &image,&image_scaled,scaled_size);   
            double elapsed = timer_get_time() - t0;
            LOGI("Downscale took %.3f ms", elapsed*1000.0);
        }

        //util_write_output(&image_scaled, "output/out_scaled.png");

        Rect rects[256] = {0};
        int num_rects = use_scaled_image ? process_image(&image_scaled, rects) : process_image(&image, rects);
        LOGI("Found %d rects", num_rects);

        if(use_scaled_image)
        {
            // correct rects positions / sizes
            const float scale = image.w > image.h ? image.w / (float)image_scaled.w : image.h / (float)image_scaled.h;
            for(int i = 0; i < num_rects; ++i)
            {
                Rect* r = &rects[i];
                r->x = (u16)round(r->x * scale);
                r->y = (u16)round(r->y * scale);
                r->w = (u16)round(r->w * scale);
                r->h = (u16)round(r->h * scale);
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
                transform_draw_rect(&image, rects[i],(Color){255,0,255,255}, false, 1.0);
            }
        }

        String outfile = StringFormat(scratch, "output/%s", settings.input_files[i].filename);
        LOGI("outfile %d: %.*s", i, outfile.len, outfile.data);
        util_write_output(&image, outfile.data);
    }

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
    settings.has_texture = false;
    settings.no_scale = false;
    settings.block_scale = 0.20;
    settings.input_file_count = 0;

    bool parse = parse_args(&settings, argc, args);
    if(!parse) return false;

    // print settings
    LOGI("--- Settings ---");
    LOGI("  Thread Count: %d", settings.thread_count);
    LOGI("  Confidence Threshold: %d", settings.confidence_threshold);
    LOGI("  NMS IOU Threshold: %f", settings.nms_iou_threshold);
    LOGI("  Texture: %s", settings.has_texture ? settings.texture_image_path : "(None)");
    LOGI("  Block Scale: %f", settings.block_scale);
    LOGI("  Debug: %s", settings.debug ? "ON" : "OFF");
    LOGI("----------------");
    
    // initialize memory arenas used in program
    for(int i = 0; i < settings.thread_count; ++i)
    {
        arenas[i] = arena_create(ARENA_SIZE_MEDIUM);
    }
    scratch = arena_create(1024*1024);

    // initialize model data
    detect_init();

    return true;
}

void print_help()
{
    printf("\n[USAGE]\n");
    printf("  censorman <in_file> -o <out_file> -d {class_list} -t {transform_list} [-c confidence_threshold][-k thread_count] [--debug] [--image <texture_image_path>] [--block_scale <block_scale>] [--is_quiet]\n");
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
                    if(STR_EQUAL(&argv[i][2],"quiet"))
                        is_quiet = true;
                    if(STR_EQUAL(&argv[i][2],"no_scale"))
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
                    else if(STR_EQUAL(&argv[i][2],"image"))
                    {
                        if(i < argc-1)
                        {
                            i++;
                            strncpy(settings->texture_image_path, argv[i], 255);
                            settings->has_texture = true;
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
                                process = true;

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
                case 'k': {
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
