#include <stdio.h>
#include <pthread.h>

#include "base.h"
#include "detect.h"
#include "transform.h"
#include "util.h"

// TODO
//
// [x] Add scramble transform
// [ ] Add padding to sub-images
// [ ] Add blur transform
// [ ] Add image scaling function
// [ ] Add lots of test images and --tester mode
// [ ] Implement thread pool (spin-lock)
// [ ] Open a video file and read image frames
// [ ] Write output video file

#define MAX_ARENAS 64
Arena* arenas[MAX_ARENAS] = {0};
Timer timer = {0};
ProgramState state = {};
pthread_t *threads = NULL;

bool init();
bool parse_args(ProgramState* state, int argc, char* argv[]);
int process_image(Image* image,Rect* ret_rects);
void apply_transform(Image* image, int num_rects, Rect* rects, TransformType transform);

int main(int argc, char** args)
{
    bool initialized = init();
    if(!initialized)
    {
        LOGE("Failed to initialize");
        return 1;
    }

    // initialize default program state
    state.mode = MODE_LOCAL;
    memset(state.input_file,0,256);
    state.num_threads = MAX(1, util_get_core_count()); // default to num_cores
    state.asset_type = TYPE_IMAGE; // @TODO
    state.classification = CLASS_FACE;
    state.transform_count = 0;
    state.debug = false;
    state.confidence_threshold = 80;
    state.nms_iou_threshold = 0.6;

    bool parse = parse_args(&state, argc, args);
    if(!parse) return 1;

    if(state.debug)
    {
        LOGI(" === DEBUGGING: ON === ");
    }

    // initialize threads
    threads = (pthread_t *)calloc(state.num_threads,sizeof(pthread_t));
    
    if(state.mode == MODE_LOCAL)
    {
        if(state.asset_type == TYPE_IMAGE)
        {
            Image image = {};
            bool loaded = util_load_image(state.input_file, &image);
            if(!loaded) return 1;

            Rect rects[256] = {0};
            int num_rects = process_image(&image, rects);
            LOGI("Found %d rects", num_rects);

            for(int i = 0; i < state.transform_count; ++i)
            {
                Transform* t = &state.transforms[i];
                LOGI("Applying %s transform...", transform_type_to_str(t->type));
                apply_transform(&image, num_rects, rects,t->type);
            }

            if(state.debug)
            {
                // draw debugging info on image
                for(int i = 0 ; i < num_rects; ++i)
                {
                    transform_draw_rect(&image, rects[i],(Color){255,0,255,255}, false, 1.0);
                }
            }

            util_write_output(&image, "output/out.png");
        }
    }

    return 0;
}

bool init()
{
    // init
    timer_init();
    log_init(0);

    time_t t;
    srand((unsigned) time(&t));

    return true;
}

// Returns number of rects
int process_image(Image* image,Rect* ret_rects)
{
    if(!threads) return 0;

    reverse_rgb_order(image);

    // Determine image subdivision

    bool is_horiz = (image->w >= image->h);

    int rows = 0;
    int cols = 0;

    switch(state.num_threads)
    {
        case 1:  rows = 1; cols = 1; break;
        case 2:  rows = is_horiz ? 1 : 2; cols = is_horiz ? 2 : 1; break;
        case 3:  rows = is_horiz ? 1 : 3; cols = is_horiz ? 3 : 1; break;
        case 4:  rows = 2; cols = 2; break;
        case 5:  rows = is_horiz ? 1 : 5; cols = is_horiz ? 5 : 1; break;
        case 6:  rows = is_horiz ? 2 : 3; cols = is_horiz ? 3 : 2; break;
        case 7:  rows = is_horiz ? 1 : 7; cols = is_horiz ? 7 : 1; break;
        case 8:  rows = is_horiz ? 2 : 4; cols = is_horiz ? 4 : 2; break;
        case 9:  rows = 3; cols = 3; break;
        case 10: rows = is_horiz ? 2 : 5;  cols = is_horiz ? 5  : 2; break;
        case 11: rows = is_horiz ? 1 : 11; cols = is_horiz ? 11 : 1; break;
        case 12: rows = is_horiz ? 3 : 4;  cols = is_horiz ? 4  : 3; break;
        case 13: rows = is_horiz ? 1 : 13; cols = is_horiz ? 13 : 1; break;
        case 14: rows = is_horiz ? 2 : 7;  cols = is_horiz ? 7  : 2; break;
        case 15: rows = is_horiz ? 3 : 5;  cols = is_horiz ? 5  : 3; break;
        case 16: rows = 4; cols = 4; break;
        default:
            rows = is_horiz ? 1 : state.num_threads;
            cols = is_horiz ? state.num_threads : 1;
    }

    int sub_width  = ceil(image->w / cols);
    int sub_height = ceil(image->h / rows);

    LOGI("Image sub-size: (%d, %d), config: %dx%d", sub_width, sub_height, rows, cols);

    Image* sub_images[state.num_threads] = {0};
    u8 detect_buffers[state.num_threads][0x9000] = {0};

    for(int i = 0; i < state.num_threads; ++i)
    {
        arenas[i] = arena_create(ARENA_SIZE_MEDIUM);
        sub_images[i] = (Image*)arena_alloc(arenas[i], sizeof(Image));
    }

    int actual_thread_count = 0;
    int x = 0;
    int y = 0;

    LOGI("Detecting faces... (threads: %d)", state.num_threads);

    facedetect_init();

    const float padding_factor = 0.1;
    int padding = MAX(sub_width, sub_height)*padding_factor;

    timer_begin(&timer);

    for(int i = 0; i < state.num_threads; ++i)
    {
        Arena* arena = arenas[actual_thread_count];
        pthread_t* thread = &threads[actual_thread_count];
        Image* sub_image = sub_images[actual_thread_count];

        // calculate offset into base image
        int offset = (y*image->w*sub_height*image->n) + x*sub_width*image->n;

        sub_image->detect_buffer = detect_buffers[actual_thread_count];
        sub_image->data = image->data + offset;
        sub_image->w = sub_width;
        sub_image->h = sub_height;
        sub_image->n = image->n;
        sub_image->step = image->w*image->n;
        sub_image->arena = arena;
        sub_image->subx = x;
        sub_image->suby = y;

        if(pthread_create(thread, NULL, detect_faces, (void*)sub_image) == 0)
        {
            //LOGI("Thread %d started (%d, %d)", i, x, y);
            actual_thread_count++;
        }
        else
        {
            LOGW("Failed to start thread");
        }

        x++;
        if(x >= cols)
        {
            x = 0;
            y++;
        }
    }

    for(int i = 0; i < state.num_threads; ++i)
    {
        //LOGI("Thread %d joined", i);
        pthread_join(threads[i], NULL);
    }

    double detection_time = timer_get_elapsed(&timer);
    LOGI("detection time: %.3f ms", detection_time*1000.0f);

    Rect total_rects[1024] = {0};
    int num_faces = 0;

    // collect face box results
    for(int i = 0; i < actual_thread_count; ++i)
    {
        Image* sub_image = sub_images[i];
        if(sub_image && sub_image->result)
        {
            u8* ret_rects = sub_image->result;
            int offset = 0;
            int sub_faces_found = *((int*)(ret_rects));
            offset += sizeof(int);

            for(int j = 0; j < sub_faces_found; ++j)
            {
                Rect* r = (Rect*)(ret_rects+offset);
                if(r->confidence < state.confidence_threshold) // filter out low-confidence regions
                    continue;

                memcpy(&total_rects[num_faces],r,sizeof(Rect));
                offset += sizeof(Rect);
                num_faces++;
            }
        }
    }

    reverse_rgb_order(image);

    // sort and filter out detected boxes
    util_sort_rects(num_faces, total_rects, false);

    // NMS (Non-Maximum Suppression)
    // Conlidate detection regions

    bool removed_rects[1024] = {0};
    int num_removed = 0;

    for(int i = 0; i < num_faces; ++i)
    {
        if(removed_rects[i])
            continue;

        Rect* a = &total_rects[i];

        for(int j = i+1; j < num_faces; ++j)
        {
            Rect* b = &total_rects[j];
            float iou = calc_iou(a,b);

            if(iou > state.nms_iou_threshold)
            {
                // remove the less confidence box
                int idx = (a->confidence < b->confidence ? i : j);
                removed_rects[idx] = true;
                num_removed++;
            }
        }
    }

    LOGI("NMS removed %d rects", num_removed);

    int ret_rects_count = 0;
    for(int i  = 0; i < num_faces; ++i)
    {
        if(removed_rects[i])
            continue;

        memcpy(&ret_rects[ret_rects_count++], &total_rects[i], sizeof(Rect));
    }

    return ret_rects_count;
}

void apply_transform(Image* image, int num_rects, Rect* rects, TransformType transform)
{
    // apply transformation
    for(int i = 0; i < num_rects; ++i)
    {
        Rect r = rects[i];
        LOGI("Rect: [%u,%u,%u,%u] confidence: %u", r.x, r.y, r.w, r.h, r.confidence);

        switch(transform)
        {
            case TRANSFORM_TYPE_BLACKOUT:       transform_draw_rect(image, r,(Color){0,0,0,255}, true, 1.0); break;
            case TRANSFORM_TYPE_PIXELATE:       transform_pixelate(image, r, 0.25); break;
            case TRANSFORM_TYPE_SCRAMBLE:       transform_scramble(image, r, 0);    break;
            case TRANSFORM_TYPE_SCRAMBLE_FIXED: transform_scramble(image, r, 409);  break; // @TODO
            case TRANSFORM_TYPE_BLUR: break;
            default: break;
        }
    }
}

void print_help()
{
    printf("censorman <file> -o <output> -d {class_list} -t {transform_list} [-k num_threads] [--debug]\n");
}

bool parse_args(ProgramState* state, int argc, char* argv[])
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
                    if(STR_EQUAL(&argv[i][2],"debug"))
                        state->debug = true;
                    break;
                case 'o':
                    break;
                case 'd':
                    break;
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

                                if(STR_EQUAL(buf, "blackout")) type = TRANSFORM_TYPE_BLACKOUT;
                                else if(STR_EQUAL(buf, "blur")) type = TRANSFORM_TYPE_BLUR;
                                else if(STR_EQUAL(buf, "pixelate")) type = TRANSFORM_TYPE_PIXELATE;
                                else if(STR_EQUAL(buf, "scramble")) type = TRANSFORM_TYPE_SCRAMBLE;

                                memset(buf,256,0);
                                bufi = 0;

                                if(type != TRANSFORM_TYPE_NONE)
                                {
                                    Transform *t = &state->transforms[state->transform_count++];
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
                        state->num_threads = n == 0 ? state->num_threads : n;
                    }
                }   break;
                default:
                    break;
            }
        }
        else if(input_file_needed)
        {
            // assume input file
            strncpy(state->input_file, argv[i], 255);
            input_file_needed = false;
        }
    }

    return true;
}
