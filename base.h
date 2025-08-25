#pragma once

#define PLATFORM_WINDOWS  1
#define PLATFORM_MAC      2
#define PLATFORM_UNIX     3

#if defined(_WIN32)
#define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PLATFORM PLATFORM_MAC
#else
#define PLATFORM PLATFORM_UNIX
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#if PLATFORM == PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <profileapi.h>
#include <handleapi.h>
#else
#include <unistd.h> // for usleep
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <assert.h>
#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// Types
// 

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    i8;
typedef int16_t   i16;
typedef int32_t   i32;
typedef int64_t   i64;
typedef float     f32;
typedef double    f64;
typedef int8_t    b8;
typedef int16_t   b16;
typedef int32_t   b32;
typedef int64_t   b64;
typedef wchar_t   wchar;

// Arenas

#define ARENA_SIZE_TINY        16*1024 //  16K
#define ARENA_SIZE_SMALL      128*1024 // 128K
#define ARENA_SIZE_MEDIUM  1*1024*1024 //   1M
#define ARENA_SIZE_LARGE  16*1024*1024 //  16M

#define ARENA_GROWTH_SIZE ARENA_SIZE_MEDIUM

typedef struct Arena
{
    u8* base;
    size_t capacity;
    size_t offset;
    struct Arena *next; // used for chaining arenas together
} Arena;

Arena *arena_create(size_t capacity)
{
    Arena *a = (Arena*)malloc(sizeof(Arena));
    if(!a) return NULL;

    a->base = (u8*)malloc(capacity * sizeof(u8));
    a->capacity = capacity;
    a->offset = 0;
    a->next = NULL;

    return a;
}

void arena_destroy(Arena* arena)
{
    if(!arena) return;

    Arena* a = arena;

    for(;;)
    {
        if(a->base) free(a->base);

        a->base = NULL;
        a->capacity = 0;
        a->offset = 0;

        if(a->next)
        {
            Arena* tmp = a;
            a = a->next;
            free(tmp);
            continue;
        }

        break;
    }

    arena = NULL;
}

void* arena_alloc(Arena* arena, size_t size)
{
    assert(arena);

    Arena* a = arena;

    for(;;)
    {
        if(a->offset + size <= a->capacity)
            break; // enough space, we're good

        // can't fit data on current arena
        // check for a next arena
        if(a->next)
        {
            a = a->next;
            continue;
        }

        // allocate a new arena that doubles the arena base capacity
        // or more to accommodate a large allocation
        
        size_t new_arena_size = (a->capacity >= size ? a->capacity : size);

        a->next = (Arena*)malloc(sizeof(Arena));
        a->next->base = (u8*)malloc(new_arena_size * sizeof(u8));
        a->next->offset = 0;
        a->next->capacity = new_arena_size;
    }

    void* ptr = a->base+a->offset;
    a->offset += size;

    return ptr;
}

void arena_reset(Arena* arena)
{
    assert(arena);

    Arena* a = arena;
    for(;;)
    {
        a->offset = 0;
        if(a->next)
        {
            a = a->next;   
            continue;
        }
        break;
    }
}

//
// Math
//
#define PI 3.14159265358979323846
#define ABS(x)   ((x) < 0 ? -(x) : (x))
#define ABSF(x)  ((x) < 0.0 ? -(x) : (x))
#define MIN(x,y) ((x)  < (y) ? (x) : (y))
#define MAX(x,y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lo, hi) MAX(MIN((x), (hi)),(lo))

//
// Strings
//
#define STR_EMPTY(x)      (x == 0 || strlen(x) == 0)
#define STR_EQUAL(x,y)    (strncmp((x),(y),strlen((x))) == 0 && strlen(x) == strlen(y))
#define STRN_EQUAL(x,y,n) (strncmp((x),(y),(n)) == 0)

#define S(literal) (String){ .len = sizeof(literal) - 1, .data = (char*)(literal) }

typedef struct
{
    u64 len;
    char* data;
} String;

String str_from_cstr(char* cstr)
{
    return (String){ .len = (u32)strlen(cstr), .data = (char*)cstr };
}

b32 str_ends_with(String str, String suffix) {
    if (suffix.len > str.len) return 0;
    return (strncmp(str.data + (str.len - suffix.len), suffix.data, suffix.len) == 0);
}

String StringFormat(Arena* arena, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int required_len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_len < 0)
    {
        return (String){ .len = 0, .data = NULL };
    }

    // Allocate from arena (+1 for null terminator)
    char* buffer = (char*)arena_alloc(arena, required_len + 1);
    if (!buffer)
    {
        return (String){ .len = 0, .data = NULL };
    }

    va_start(args, format);
    vsnprintf(buffer, required_len + 1, format, args);
    va_end(args);

    return (String){ .len = (u32)required_len, .data = buffer };
}

int str_get_extension(const char *source, char *buf, int buf_len)
{
    if (!source || !buf) return 0;

    int len = strlen(source);
    if (len == 0) return 0;

    // Start from the end and move backward
    for (int i = len - 1; i >= 0; i--) {
        if (source[i] == '.')
        {
            // If '.' is the last character, no extension
            if (i == len - 1) return 0;

            // Copy extension
            int copy_len = MIN(len-i-1,buf_len-1);
            strncpy(buf, &source[i+1], copy_len);
            buf[copy_len] = '\0';
            return strlen(buf);
        }
    }

    return 0;
}

//
// Util
//
#define DEBUG()   printf("[DEBUG] %s %s(): %d\n", __FILE__, __func__, __LINE__)



//
// Arrays
//

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))


//
// Timer 
//

typedef struct
{
    double time_start;
    double time_last;
} Timer;

void timer_init(void);

void timer_begin(Timer* timer);
double timer_get_elapsed(Timer* timer);
void timer_delay_us(int us);
double timer_get_time();

static struct
{
    bool monotonic;
    uint64_t  frequency;
    uint64_t  offset;
} _timer;

#if _WIN32
void usleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, 1, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#endif

static uint64_t get_timer_value(void)
{
#if _WIN32
    uint64_t counter;
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);
    return counter;
#else
#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    if (_timer.monotonic)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
    }
    else
#endif

    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t) tv.tv_sec * (uint64_t) 1000000 + (uint64_t) tv.tv_usec;

    }
#endif
}

void timer_init(void)
{
#if _WIN32
    uint64_t freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    _timer.monotonic = false;
    _timer.frequency = freq;
#else

    srand(time(NULL));

#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    {
        _timer.monotonic = true;
        _timer.frequency = 1000000000;
    }
    else
#endif
    {
        _timer.monotonic = false;
        _timer.frequency = 1000000;
    }
#endif
    _timer.offset = get_timer_value();

}

static double get_time()
{
    return (double) (get_timer_value() - _timer.offset) / (double)_timer.frequency;
}

void timer_begin(Timer* timer)
{
    timer->time_start = get_time();
    timer->time_last = timer->time_start;
}

double timer_get_time()
{
    return get_time();
}

double timer_get_elapsed(Timer* timer)
{
    double time_curr = get_time();
    return time_curr - timer->time_start;
}

void timer_delay_us(int us)
{
    usleep(us);
}

// Logging

#define LOG_COLOR_BLACK   "30"
#define LOG_COLOR_RED     "31"
#define LOG_COLOR_GREEN   "32"
#define LOG_COLOR_BROWN   "33"
#define LOG_COLOR_BLUE    "34"
#define LOG_COLOR_PURPLE  "35"
#define LOG_COLOR_CYAN    "36"
#define LOG_COLOR_WHITE   "37"
#define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define LOG_RESET_COLOR   "\033[0m"
#define LOG_COLOR_E       LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W       LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I       LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D       LOG_COLOR(LOG_COLOR_PURPLE)
#define LOG_COLOR_V       LOG_COLOR(LOG_COLOR_CYAN)
#define LOG_COLOR_N       LOG_COLOR(LOG_COLOR_WHITE)

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#if defined(WIN32)

#define LOG_FMT_START(letter)   #letter " [" "%-10.10s:%4d " "%7.2f ]: "
#define LOG_FMT_END()           "\n"
#define LOG_FMT(letter, format) LOG_FMT_START(letter) format LOG_FMT_END()

#else

#define LOG_FMT_START(letter)     LOG_COLOR_ ## letter #letter LOG_RESET_COLOR " [" LOG_COLOR(LOG_COLOR_BLUE) "%-10.10s:%4d " LOG_RESET_COLOR "%7.2f ]: " LOG_COLOR_ ## letter
#define LOG_FMT_END()           LOG_RESET_COLOR "\n"
#define LOG_FMT(letter, format) LOG_FMT_START(letter) format LOG_FMT_END()

#endif

static bool is_quiet = false;

static Timer log_timer = {0};
static void log_init(int log_level)
{
    timer_begin(&log_timer);
}

static void print_log(const char* fmt, ...)
{
    if(is_quiet) return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#define LOG(format, ...) print_log(format, __FILENAME__, __LINE__, timer_get_elapsed(&log_timer), ##__VA_ARGS__)

#define LOGE(format,...) LOG(LOG_FMT(E, format), ##__VA_ARGS__) // error
#define LOGW(format,...) LOG(LOG_FMT(W, format), ##__VA_ARGS__) // warning
#define LOGI(format,...) LOG(LOG_FMT(I, format), ##__VA_ARGS__) // info
#define LOGV(format,...) LOG(LOG_FMT(V, format), ##__VA_ARGS__) // verbose
#define LOGN(format,...) LOG(LOG_FMT(N, format), ##__VA_ARGS__) // network

//
// Program-specific types
//

typedef enum
{
    TYPE_IMAGE = 0,
    TYPE_VIDEO,
} AssetType;

typedef enum
{
    CLASS_FACE = 0,
} DetectClass;

typedef enum
{
    TRANSFORM_TYPE_NONE = 0,
    TRANSFORM_TYPE_BLACKOUT,
    TRANSFORM_TYPE_BLUR,
    TRANSFORM_TYPE_PIXELATE,
    TRANSFORM_TYPE_SCRAMBLE,
    TRANSFORM_TYPE_SCRAMBLE_FIXED,
    TRANSFORM_TYPE_TEXTURE,
} TransformType;

inline const char* transform_type_to_str(TransformType t)
{
    switch(t)
    {
        case TRANSFORM_TYPE_NONE: return "None";
        case TRANSFORM_TYPE_BLACKOUT: return "Black Out";
        case TRANSFORM_TYPE_BLUR: return "Blur";
        case TRANSFORM_TYPE_PIXELATE: return "Pixelate";
        case TRANSFORM_TYPE_SCRAMBLE: return "Scramble";
        case TRANSFORM_TYPE_SCRAMBLE_FIXED: return "Scramble (Fixed Seed)";
        case TRANSFORM_TYPE_TEXTURE: return "Texture";
        default: return "Unknown";
    }
}


typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
    u16 confidence;
} Rect;

typedef struct
{
    u8 *data;
    int w;
    int h;
    int n; // channels
    int step; // number of bytes to advance to next row

    // used for sub-image thread processing
    u8 *detect_buffer;
    u8 subx; // position in larger image
    u8 suby; // position in larger image
    void* arena;
    bool scaled; // determine if image was scaled
    u32 frame_number; // used for video reconstruction
    u8* result;
} Image;

typedef struct {
    u16 w;
    u16 h;
    u32 frame_count;
    u8* data; // RGB
} Video;

typedef struct
{
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} Color;

typedef struct
{
    TransformType type;
    // ...
} Transform;

typedef struct
{
    char filename[101];
    Image image;
} InputFile;

typedef struct
{
    AssetType asset_type;
    DetectClass classification;

    Transform transforms[10];
    int transform_count;

    char input_file_text[256];
    char input_directory[256];
    InputFile input_files[100];
    int input_file_count;
    int thread_count;

    u16 confidence_threshold;
    float nms_iou_threshold;

    bool has_texture;
    char texture_image_path[256];

    float block_scale;

    bool no_scale;
    bool debug;
} ProgramSettings;

#define MAX_FRAMES 1000
#define MAX_ARENAS 64

extern ProgramSettings settings;
extern pthread_t *threads;
extern Timer timer;
extern Arena* thread_arenas[MAX_ARENAS];
extern Image texture_image;

                                                          
#ifdef __cplusplus
}
#endif

