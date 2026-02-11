#pragma once

#define OS_WINDOWS  1
#define OS_MAC      2
#define OS_UNIX     3

#if defined(_WIN32)
#define OS OS_WINDOWS
#elif defined(__APPLE__)
#define OS OS_MAC
#else
#define OS OS_UNIX
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#if OS == OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <profileapi.h>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <pthread.h>
#else
#include <unistd.h> // for usleep
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#endif

#include <assert.h>
#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif


//:==================================
// Types
//:==================================

typedef uint8_t   u8;
typedef uint16_t  u16;
typedef uint32_t  u32;
typedef uint64_t  u64;
typedef int8_t    s8;
typedef int16_t   s16;
typedef int32_t   s32;
typedef int64_t   s64;
typedef float     f32;
typedef double    f64;
typedef int8_t    b8;
typedef int16_t   b16;
typedef int32_t   b32;
typedef int64_t   b64;
typedef wchar_t   wchar;

//:==================================
// Debugging
//:==================================

#define DEBUG()   printf("[DEBUG] %s %s(): %d\n", __FILE__, __func__, __LINE__)

//:==================================
// Bit-wise helpers
//:==================================

#define BIT_SET(base,n)    ((base) |= (1UL<<(n)))
#define BIT_CLR(base,n)    ((base) &= ~(1UL<<(n)))
#define BIT_FLIP(base,n)   ((base) ^= (1UL<<(n)))
#define BIT_CHECK(base,n)  ((base) & (n) == (n))
#define BIT_IS_SET(base,n) ((base) & (1UL<<(n)))

//:==================================
// Math
//:==================================

#define PI 3.14159265358979323846
#define ABS(x)   ((x) < 0 ? -(x) : (x))
#define ABSF(x)  ((x) < 0.0 ? -(x) : (x))
#define MIN(x,y) ((x)  < (y) ? (x) : (y))
#define MAX(x,y) ((x) >= (y) ? (x) : (y))
#define CLAMP(x, lo, hi) MAX(MIN((x), (hi)),(lo))
#define SWAP(T, a, b) do { T temp = a; a = b; b = temp; } while (0)
#define BETWEEN(x,min,max) ((x) >= (min) && (x) <= (max))

f32 exp_decay(f32 a, f32 b, f32 decay, f32 dt)
{
    return b + (a - b)*exp(-decay*dt);
}

f32 lerp(f32 a, f32 b, f32 t)
{
    t = CLAMP(t,0.0,1.0);
    f32 r = (1.0-t)*a+(t*b);
    return r;
}

f32 exponential_smooth(f32 start, f32 end, f32 alpha, s32 frame)
{
    alpha = CLAMP(alpha, 0.0f, 1.0f);
    f32 factor = powf(1.0f - alpha, frame + 1);
    return end - (end - start) * factor;
}

//===================================
// Utility
//===================================

#define KB(n)  (((u64)(n)) << 10)
#define MB(n)  (((u64)(n)) << 20)
#define GB(n)  (((u64)(n)) << 30)
#define TB(n)  (((u64)(n)) << 40)

#define ALIGN_UP_POW2(x,b) (((x) + (b) - 1) & (~((b) - 1)))

//:==================================
// Memory
//:==================================

#define MemoryZero(p,z) memset((p), 0, (z))
#define MemoryZeroStruct(p) MemoryZero((p), sizeof(*(p)))

#define MemoryCopy(d,s,z) memmove((d), (s), (z))
#define MemoryCopyStruct(d,s) MemoryCopy((d), (s), MIN(sizeof(*(d)), sizeof(*(s))))

//:==================================
// Arenas
//:==================================

#define PUSH_ONE(arena,T)    arena_push((arena), sizeof(T), false)
#define PUSH_ONE_NZ(arena,T) arena_push((arena), sizeof(T), true)

#define PUSH_ARRAY(arena,T,n)    arena_push((arena), (n) * sizeof(T), false)
#define PUSH_ARRAY_NZ(arena,T,n) arena_push((arena), (n) * sizeof(T), true)

#define ARENA_ALIGN sizeof(void*)

typedef struct Arena Arena;
struct Arena
{
    u8* memory;         // pointer to beginning of memory block
    u64 capacity;       // the total size of the memory block
    u64 offset;         // current offset in memory block
    u64 base_pos;       // absolute position for this arena's memory
    Arena *next;        // used for chaining arenas together
};

typedef struct
{
    Arena *arena;
    u64 start_pos;
} ArenaTemp;

typedef ArenaTemp Temp;

// global scratch arena
Arena *_scratch_arena = {0};

Arena *arena_create(u64 capacity)
{
    Arena *a = (Arena *)malloc(sizeof(Arena));

    a->memory = (u8*)malloc(capacity);
    a->capacity = capacity;
    a->offset = 0;
    a->base_pos = 0;
    a->next = NULL;

    return a;
}

void arena_destroy(Arena *arena)
{
    if(!arena) return;

    for(;;)
    {
        if(arena->memory) free(arena->memory);

        arena->memory = NULL;
        arena->capacity = 0;
        arena->offset = 0;
        arena->base_pos = 0;

        if(arena->next)
        {
            Arena* tmp = arena;
            arena = arena->next;
            free(tmp);
            continue;
        }

        break;
    }

    arena = NULL;
}

void* arena_push(Arena *arena, u64 size, b32 non_zero)
{
    assert(arena);

    u64 offset_aligned = ALIGN_UP_POW2(arena->offset, ARENA_ALIGN);

    for(;;)
    {
        if(offset_aligned + size <= arena->capacity)
            break; // enough space, we're good

        // can't fit data on current arena
        // check for a next arena
        if(arena->next)
        {
            arena = arena->next;
            continue;
        }

        // allocate a new arena that doubles the arena memory capacity
        // or more to accommodate a large allocation
        
        u64 new_arena_size = (arena->capacity >= size ? 2*arena->capacity : size);

        arena->next = (Arena*)malloc(sizeof(Arena));
        arena->next->memory = (u8*)malloc(new_arena_size * sizeof(u8));
        arena->next->offset = 0;
        arena->next->base_pos = arena->base_pos + arena->capacity;
        arena->next->capacity = new_arena_size;
        arena->next->next = NULL;
    }

    void *ptr = arena->memory + offset_aligned;
    arena->offset = offset_aligned + size;

    if(!non_zero) MemoryZero(ptr, size);

    return ptr;
}

u64 arena_pos(Arena *arena)
{
    return arena->base_pos + arena->offset;
}


void arena_pop_to(Arena *arena, u64 pos)
{
    for(;;)
    {
        if(BETWEEN(pos, arena->base_pos, arena_pos(arena)))
        {
            arena->offset = (pos - arena->base_pos);
        }
        else if(pos < arena->base_pos)
        {
            arena->offset = 0;
        }

        if(arena->next)
        {
            arena = arena->next;
            continue;
        }
        break;
    }
}

ArenaTemp arena_temp_begin(Arena *arena)
{
    return (ArenaTemp){.arena = arena, .start_pos = arena_pos(arena)};
}

void arena_temp_end(ArenaTemp temp)
{
    arena_pop_to(temp.arena, temp.start_pos);
}

ArenaTemp scratch_begin(void)
{
    if(_scratch_arena == NULL)
    {
        _scratch_arena = arena_create(MB(64));
    }

    return arena_temp_begin(_scratch_arena);

}

void scratch_end(ArenaTemp scratch)
{
    arena_temp_end(scratch);
}


void arena_reset(Arena *arena)
{
    for(;;)
    {
        arena->offset = 0;

        if(arena->next)
        {
            arena = arena->next;   
            continue;
        }
        break;
    }
}

//:==================================
// Timer
//:==================================

typedef struct
{
    f64 time_start;
    f64 time_last;
} Timer;

void timer_init(void);

void timer_begin(Timer* timer);
f64 timer_get_elapsed(Timer* timer);
void timer_delay_us(s32 us);
f64 timer_get_time();

static struct
{
    b32 monotonic;
    u64 frequency;
    u64 offset;
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

static u64 get_timer_value(void)
{
#if _WIN32
    u64 counter;
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);
    return counter;
#else
#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
    if (_timer.monotonic)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (u64)ts.tv_sec * (u64)1000000000 + (u64)ts.tv_nsec;
    }
    else
#endif

    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (u64) tv.tv_sec * (u64) 1000000 + (u64) tv.tv_usec;

    }
#endif
}

void timer_init(void)
{
#if _WIN32
    u64 freq;
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

static f64 get_time()
{
    return (f64) (get_timer_value() - _timer.offset) / (f64)_timer.frequency;
}

void timer_begin(Timer* timer)
{
    timer->time_start = get_time();
    timer->time_last = timer->time_start;
}

f64 timer_get_time()
{
    return get_time();
}

f64 timer_get_elapsed(Timer* timer)
{
    f64 time_curr = get_time();
    return time_curr - timer->time_start;
}

void timer_delay_us(s32 us)
{
    usleep(us);
}

f64 __stopwatch_t0;

void stopwatch_start()
{
    __stopwatch_t0 = timer_get_time();
}

f64 stopwatch_time()
{
    return (timer_get_time() - __stopwatch_t0);
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

typedef enum
{
    LOG_TYPE_NONE = 0,
    LOG_TYPE_ERROR,
    LOG_TYPE_WARNING,
    LOG_TYPE_NETWORK,
    LOG_TYPE_INFO,
    LOG_TYPE_VERBOSE,
} LogType;

LogType log_level = LOG_TYPE_INFO;
static b32 is_quiet = false;

static Timer log_timer = {};
static void log_init(s32 log_level)
{
    timer_begin(&log_timer);
}

static void print_log(LogType type, const char* fmt, ...)
{
    if(is_quiet) return;
    if(type > log_level) return;

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

#define log(type, format, ...) print_log(type, format, __FILENAME__, __LINE__, timer_get_elapsed(&log_timer), ##__VA_ARGS__)

#define loge(format,...) log(LOG_TYPE_ERROR,   LOG_FMT(E, format), ##__VA_ARGS__) // error
#define logw(format,...) log(LOG_TYPE_WARNING, LOG_FMT(W, format), ##__VA_ARGS__) // warning
#define logi(format,...) log(LOG_TYPE_INFO   , LOG_FMT(I, format), ##__VA_ARGS__) // info
#define logv(format,...) log(LOG_TYPE_VERBOSE, LOG_FMT(V, format), ##__VA_ARGS__) // verbose
#define logn(format,...) log(LOG_TYPE_NETWORK, LOG_FMT(N, format), ##__VA_ARGS__) // network

//:==================================
// Strings
//:==================================

#define S(literal)      (String){sizeof(literal)-1, (u8*)(literal)}
#define STR(cstr)       (String){cstring_strlen(cstr),(u8*)(cstr)}

#define STR_EQUAL(x,y)  (strncmp((x),(y),strlen((x))) == 0 && strlen(x) == strlen(y)) 
#define STR_EMPTY(x)    ((x) == 0 || cstring_strlen(x) == 0)
#define STR_BOOL(b)     ((b) ? "True" : "False")

#define STR_FMT "%.*s"
#define STR_ARG(s) s.len, s.data

typedef struct
{
    u64 len;
    u8* data;
} String;

typedef struct
{
    u64 count;
    String *items;
} StringArray;

typedef struct StringNode StringNode;
struct StringNode
{
    String str;
    StringNode *prev;
    StringNode *next;
};

typedef struct
{
    StringNode *head;
    StringNode *last;
    u64 count;
    Arena *arena;
} StringList;

b32 char_is_whitespace(u8 c);
b32 char_is_digit(u8 c);
b32 char_is_alpha(u8 c);
b32 char_is_lower(u8 c);
b32 char_is_upper(u8 c);
u8 char_to_lower(u8 c);
u8 char_to_upper(u8 c);

String string_format(Arena *arena, const char *format, ...);
String string_concat(Arena *arena, s32 count, ...);

String string_nil();

b32 string_equal(String s, String t);
b32 string_starts_with(String str, String start);
b32 string_ends_with(String str, String end);

String string_substring(String s, u64 start, u64 len);
String string_trim(String s);
s64 string_get_first_index(String s, const char *find, b32 from_end);
b32 string_contains(String s, String find);
String string_replace(Arena *arena, String str, String find, String replacement);

b32 string_in_list(String str, StringList list);
b32 string_in_array(String str, StringArray arr);

void string_to_lower(String *str);
void string_to_upper(String *str);

// Returns null-terminated C String
char* string_to_cstr(Arena *arena, String str);

void string_print(String s);

StringArray string_array_nil();
StringArray string_array_create(Arena *arena, u64 count, ...);
StringArray string_array_create_empty(Arena *arena, u64 count);
StringArray string_split(Arena *arena, String base, String split);
StringArray string_list_to_array(StringList sl);

String string_eat_whitespace(String str);
String string_advance(String str, u64 count);
void   string_advance_in_place(String *str, u64 count);
char   string_get_char_at_index(String str, u64 index);
char   string_top_char(String str);
char   string_get_char_and_advance(String *str);
String string_advance_char(String str);

s64 string_to_s64(String str);
f64 string_to_f64(String str);

u64 cstring_strlen(const char *str);


b32 char_is_whitespace(u8 c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

b32 char_is_digit(u8 c)
{
    return (c >= '0' && c <= '9');
}

b32 char_is_alpha(u8 c)
{
    return (c >= 'A' && c <= 'z');
}

b32 char_is_lower(u8 c)
{
    return (c >= 'a' && c <= 'z');
}

b32 char_is_upper(u8 c)
{
    return (c >= 'A' && c <= 'Z');
}

u8 char_to_lower(u8 c)
{
    if(char_is_upper(c))
    {
        c += ('a' - 'A');
    }
    return c;
}

u8 char_to_upper(u8 c)
{
    if(char_is_lower(c))
    {
        c += ('A' - 'a');
    }
    return c;
}

u64 cstring_strlen(const char *str)
{
    u64 len = 0;
    for(const char *p = str; *p; ++p) len++;
    return len;
}

String string_nil()
{
    String str = {0};
    return str;
}

String string_copy(Arena *arena,String str)
{
    String ret = {0};
    ret.len = str.len;
    ret.data = (u8 *)PUSH_ARRAY(arena, u8, str.len);
    MemoryCopy(ret.data, str.data, str.len);

    return ret;
}

String string_format(Arena *arena, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    s32 required_len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_len < 0)
    {
        return (String){ .len = 0, .data = NULL };
    }

    char* buffer = (char*)PUSH_ARRAY(arena, char, required_len);
    if (!buffer)
    {
        return (String){ .len = 0, .data = NULL };
    }

    va_start(args, format);
    vsnprintf(buffer, required_len+1, format, args);
    va_end(args);

    return (String){ .len = (u32)required_len, .data = (u8 *)buffer };
}

char* string_to_cstr(Arena *arena, String str)
{
    char* cstr;
    cstr = (char*)PUSH_ARRAY(arena, char, str.len+1); // +1 for null terminator
    MemoryCopy(cstr,str.data, str.len);
    return cstr;
}

void string_print(String s)
{
    if(s.len == 0 || !s.data) return;
    logi(STR_FMT,s.len, s.data);
}

String string_concat(Arena *arena, s32 count, ...)
{
    va_list args1, args2;
    va_start(args1, count);
    va_copy(args2, args1);

    u64 total_len = 0;
    for(u64 i = 0; i < count; ++i)
    {
        String s = va_arg(args1, String);
        total_len += s.len;
    }
    va_end(args1);

    String str = {0};
    str.data = (u8 *)PUSH_ARRAY(arena, u8, total_len);

    for(s32 i = 0; i < count; ++i)
    {
        String s = va_arg(args2, String);
        MemoryCopy(&str.data[str.len],s.data, s.len);
        str.len += s.len;
    }
    va_end(args2);

    return str;
}

void string_to_lower(String *str)
{
    for(u64 i = 0; i < str->len; ++i)
    {
        str->data[i] = char_to_lower(str->data[i]);
    }
}

void string_to_upper(String *str)
{
    for(u64 i = 0; i < str->len; ++i)
    {
        str->data[i] = char_to_upper(str->data[i]);
    }
}

b32 string_starts_with(String str, String start)
{
    if(start.len > str.len)
        return false;

    String prefix = string_substring(str, 0, start.len);
    return string_equal(prefix, start);
}

b32 string_ends_with(String str, String end)
{
    if (end.len > str.len)
        return false;

    String suffix = string_substring(str, str.len - end.len, end.len);
    return string_equal(suffix, end);
}

s64 string_get_first_index(String s, const char *find, b32 from_end)
{
    String find_str = STR(find);

    for(u64 i = from_end ? s.len - 1 : 0; from_end ? i >= 0 : i < s.len; i += from_end ? -1 : 1)
    {
        if(s.data[i] == find_str.data[0])
        {
            b32 match = true;
            for(u64 j = 1; j < find_str.len; ++j)
            {
                ++i;
                if(i >= s.len) {
                    match = false;
                    break;
                }

                if(s.data[i] != find_str.data[j])
                {
                    match = false;
                    break;
                }
            }

            if(match)
            {
                return i;
            }
        }
    }

    return -1;
}

b32 string_contains(String s, String find)
{
    if(find.len == 0) return false;

    for(int i = 0; i < s.len; ++i)
    {
        String sub = string_substring(s, i,find.len);
        if(string_equal(sub, find))
        {
            return true;
        }
    }

    return false;
}

String string_replace(Arena *arena, String str, String find, String replacement)
{
    if(find.len == 0)
    {
        // return a copy of the data
        return string_copy(arena, str);
    }

    // find number of instances
    u64 *instances = (u64 *)PUSH_ARRAY(arena, u64, str.len); // total possible in indices
    u32 instance_count = 0;

    for(u64 i = 0; i < str.len; ++i)
    {
        String sub = string_substring(str, i, find.len);
        if(string_equal(sub, find))
        {
            // found instance of 'find' string
            instances[instance_count++] = i;
        }
    }

    s64 new_len = str.len + ((replacement.len - find.len)*instance_count);
    if(new_len <= 0)
    {
        return string_nil();
    }

    // allocate space for new string
    String new_str = {0};
    new_str.len = (u64)new_len;
    new_str.data = (u8 *)PUSH_ARRAY(arena, u8, new_len);

    u64 str_index = 0;
    u64 new_index = 0;

    u32 i = 0;
    for(;;)
    {
        if(i >= instance_count)
        {
            // copy any remaining string after last instance
            MemoryCopy(new_str.data + new_index , str.data + str_index , str.len - str_index);
            break;
        }
        u64 instance_index = instances[i];
        MemoryCopy(new_str.data + new_index, str.data + str_index, instance_index - str_index);
        new_index += instance_index - str_index;
        str_index = instance_index;

        MemoryCopy(new_str.data + new_index, replacement.data, replacement.len);
        new_index += replacement.len;
        str_index += find.len;

        i++;
    }

    return new_str;
}

b32 string_in_list(String str, StringList list)
{
    StringNode *sn = list.head;
    for(;;)
    {
        if(string_equal(str, sn->str))
            return true;

        if(!sn->next) break;
        sn = sn->next;
    }
    return false;
}

b32 string_in_array(String str, StringArray arr)
{
    for(s32 i = 0; i < arr.count; ++i)
    {
        if(string_equal(str, arr.items[i]))
            return true;
    }
    return false;
}

b32 string_equal(String s, String t)
{
    if(s.len != t.len) return false;

    for(u64 i = 0; i < s.len; ++i)
        if(s.data[i] != t.data[i])
            return false;

    return true;
}


String string_substring(String s, u64 start, u64 len)
{
    String ret = {
        .len = len,
        .data = s.data + start
    };
    return ret;
}

String string_trim(String s)
{
    String ret = s;

    // rtrim
    for(u8 *p = ret.data+ret.len-1; ret.len > 0 && char_is_whitespace(*p); --p, ret.len--);

    // ltrim
    for(u8 *p = ret.data; ret.len > 0 && char_is_whitespace(*p); ++p, ret.data++, ret.len--);

    return ret;
}

////////////////////////////////////
// String Conversions

f64 string_to_f64(String str)
{
    f64 f = 0.0;
    s32 e = 0;
    s32 c;
    s32 sign = 1;

    str = string_eat_whitespace(str);

    while(string_top_char(str) == '-')
    {
        str = string_advance_char(str);
        sign *= -1;
    }

    for(;;)
    {
        c = string_get_char_and_advance(&str);

        if(!char_is_digit(c)) break;

        f = f*10.0 + (c - '0');
    }

    if (c == '.')
    {
        for(;;)
        {
            c = string_get_char_and_advance(&str);
            if(!char_is_digit(c)) break;

            f = f*10.0 + (c - '0');
            e = e-1;
        }
    }

    if (c == 'e' || c == 'E')
    {
        s32 sign = 1;
        s32 i = 0;

        c = string_get_char_and_advance(&str);

        if (c == '+')
        {
            c = string_get_char_and_advance(&str);
        }
        else if (c == '-')
        {
            c = string_get_char_and_advance(&str);
            sign = -1;
        }

        for(;;)
        {
            if(!char_is_digit(c)) break;
            i = i*10 + (c - '0');
            c = string_get_char_and_advance(&str);
        }

        e += i*sign;
    }

    while (e > 0)
    {
        f *= 10.0;
        e--;
    }

    while (e < 0)
    {
        f *= 0.1;
        e++;
    }

    f *= sign;

    return f;
}

s64 string_to_s64(String str)
{
    s64 i = 0;
    s32 e = 0;
    s32 c;
    s32 sign = 1;
    str = string_eat_whitespace(str);

    for(;;)
    {
        if(string_top_char(str) != '-')
            break;

        str = string_advance_char(str);
        sign *= -1;
    }

    for(;;)
    {
        char c = string_get_char_and_advance(&str);
        if(!char_is_digit(c)) break;

        i = i*10 + (c - '0');
    }

    if (c == 'e' || c == 'E')
    {
        s32 sign = 1;
        s32 n = 0;

        c = string_get_char_and_advance(&str);

        if (c == '+')
        {
            c = string_get_char_and_advance(&str);
        }
        else if (c == '-')
        {
            c = string_get_char_and_advance(&str);
            sign = -1;
        }

        for(;;)
        {
            if(!char_is_digit(c)) break;
            n = n*10 + (c - '0');
            c = string_get_char_and_advance(&str);
        }

        e += n*sign;
    }

    while (e > 0)
    {
        i *= 10.0;
        e--;
    }

    while (e < 0)
    {
        i *= 0.1;
        e++;
    }

    i *= sign;

    return i;
}

////////////////////////////////////
// String Lists

StringList string_list_create(Arena *arena)
{
    StringList sl = {0};

    sl.head = NULL;
    sl.last = NULL;
    sl.count = 0;
    sl.arena = arena;

    return sl;
}

void string_list_add(StringList *sl, String str)
{
    if(sl->head == NULL)
    {
        // first element
        sl->head = (StringNode *)PUSH_ONE(sl->arena, StringNode);
        sl->head->prev = NULL;
        sl->head->next = NULL;
        sl->last = sl->head;
    }
    else
    {
        StringNode *prior_last = sl->last;
        sl->last->next = (StringNode *)PUSH_ONE(sl->arena, StringNode);
        sl->last = sl->last->next;
        sl->last->next = NULL;
        sl->last->prev = prior_last;
    }

    sl->last->str.data = str.data;
    sl->last->str.len = str.len;

    sl->count++;
}

void string_list_addf(StringList *sl, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    s32 required_len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    if (required_len <= 0)
    {
        return;
    }

    char* buffer = (char *)PUSH_ARRAY(sl->arena, char, required_len);
    if (!buffer)
    {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, required_len+1, format, args);
    va_end(args);

    String str = {(u32)required_len, (u8 *)buffer};

    string_list_add(sl, str);

    return;

}

b32 string_list_remove(StringList *sl, u64 index)
{
    u64 idx = 0;

    StringNode *sn = sl->head;
    b32 result = false;

    for(;;)
    {
        if(idx > index)
            break;

        if(idx == index)
        {
            // remove string node

            if(sn->prev && sn->next)
            {
                sn->prev->next = sn->next;
                sn->next->prev = sn->prev;
            }
            else
            {
                if(sn->next)
                {
                    sn->next->prev = NULL;
                    if(idx == 0) sl->head = sn->next;
                }

                if(sn->prev)
                {
                    sn->prev->next = NULL;
                    if(idx == sl->count - 1) sl->last = sn->prev;
                }
            }

            sn->prev = NULL;
            sn->next = NULL;

            result = true;

            // TODO: add to a free list to use
            //       for future allocations
        }

        if(!sn->next)
            break;

        sn = sn->next;
        idx++;
    }

    return result;
}

String string_list_get(StringList *sl, u64 index)
{
    u64 idx = 0;

    StringNode *sn = sl->head;
    b32 result = false;

    for(;;)
    {
        if(idx > index)
            break;

        if(idx == index)
            return sn->str;

        if(!sn->next)
            break;

        sn = sn->next;
        idx++;
    }

    // return empty string?
    return (String){0, NULL};
}

String string_list_collapse(StringList *sl)
{
    String str = {0};
    u64 total_size = 0;

    StringNode *sn;
   
    sn = sl->head;
    if(sn == NULL)
    {
        logw("StringNode is null");
        return str;
    }

    for(;;)
    {
        total_size += sn->str.len;

        if(!sn->next)
            break;

        sn = sn->next;
    }

    if(total_size == 0)
        return str;

    if(sl->arena)
        str.data = (u8 *)PUSH_ARRAY(sl->arena, u8, total_size);
    else
        str.data = (u8 *)malloc(total_size*sizeof(u8));

    sn = sl->head;
    for(;;)
    {
        String *it = &sn->str;
        MemoryCopy(&str.data[str.len], it->data, it->len);
        str.len += it->len;

        if(!sn->next)
            break;
        sn = sn->next;
    }

    return str;
}

String string_eat_whitespace(String str)
{
    String ret = str;

    for(;;)
    {
        if(ret.len == 0) break;
        if(!char_is_whitespace(ret.data[0])) break;
        ret.data += sizeof(u8);
        ret.len--;
    }

    return ret;
}

void string_advance_in_place(String *str, u64 count)
{
    if(str->len < count) return;

    str->len -= count;
    str->data += count;
}

String string_advance(String str, u64 count)
{
    String ret = {0};
    if(str.len < count) return ret;

    ret.len = str.len - count;
    ret.data = str.data + count;
    return ret;
}

char string_get_char_at_index(String str, u64 index)
{
    char c = str.len > 0 ? str.data[index] : '\0';
    return c;
}

char string_get_char_and_advance(String *str)
{
    char c = str->len > 0 ? str->data[0] : '\0';
    string_advance_in_place(str, 1);
    return c;
}

char string_top_char(String str)
{
    return string_get_char_at_index(str,0);
}

String string_advance_char(String str)
{
    return string_advance(str, 1);
}

StringArray string_array_nil()
{
    StringArray sa = {0};
    return sa;
}

StringArray string_array_create(Arena *arena, u64 count, ...)
{
    StringArray sa;
    sa.count = count;
    sa.items = (String *)PUSH_ARRAY(arena, String, count);

    va_list args;
    va_start(args, count);

    for(s32 i = 0; i < count; ++i)
    {
        sa.items[i] = va_arg(args, String);
    }

    va_end(args);
    return sa;
}

StringArray string_array_create_empty(Arena *arena, u64 count)
{
    StringArray sa;
    sa.count = count;
    sa.items = (String *)PUSH_ARRAY(arena, String, count);
    return sa;
}

StringArray string_list_to_array(StringList sl)
{
    StringArray sa;
    sa.count = sl.count;
    sa.items = (String *)PUSH_ARRAY(sl.arena, String, sl.count);

    StringNode *sn = sl.head;
    for(s32 i = 0; i < sl.count; ++i)
    {
        MemoryCopy(&sa.items[i], &sn->str, sizeof(String));
        if(!sn->next) break;
        sn = sn->next;
    }

    return sa;
}

StringArray string_split(Arena *arena, String base, String split)
{
    u64 num_strings = 1;
    s64 split_indices[2048] = {0}; // @NOTE: Arbitrary limit

    for(u64 i = 0; i < base.len; ++i)
    {
        if(base.data[i] == split.data[0])
        {
            b32 match = true;
            for(u64 j = 1; j < split.len; ++j)
            {
                ++i;
                if(i >= base.len) {
                    match = false;
                    break;
                }

                if(base.data[i] != split.data[j])
                {
                    match = false;
                    break;
                }
            }

            if(match)
            {
                split_indices[num_strings] = i+1;
                num_strings++;
            }
        }
    }

    split_indices[num_strings] = base.len;

    StringArray sa = {0};
    sa.items = (String *)PUSH_ARRAY(arena, String, num_strings);
    sa.count = num_strings;

    for(u64 i = 0; i < num_strings; ++i)
    {
        u64 string_len = split_indices[i+1] - split_indices[i] - split.len;
        if(i+1 == num_strings) string_len += split.len;

        String *str = &sa.items[i];
        str->data = (u8 *)PUSH_ARRAY(arena, u8, string_len);
        str->len = string_len;

        MemoryCopy(str->data, &base.data[split_indices[i]], string_len);
    }

    return sa;
}


//:==================================
// Arrays
//:==================================

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#define ARRAY_SORT(arr, type, count, desc) do { \
    type i, key, j; \
    for (i = 1; i < count; ++i) \
    { \
        key = arr[i]; \
        j = i - 1; \
        for(;;) \
        { \
            if(j < 0) break; \
            if(!desc && arr[j] <= key) break; \
            if(desc  && arr[j] >= key) break; \
            arr[j + 1] = arr[j]; \
            j = j - 1; \
        } \
        arr[j + 1] = key; \
    } \
    } while(0)


///////////////////////////////////////
// Files
///////////////////////////////////////

typedef enum
{
    OS_READABLE = (1 << 0),
    OS_WRITABLE = (1 << 1),
} OS_FileProps;

typedef struct OS_File OS_File;

#if OS == OS_WINDOWS
struct OS_File
{
    HANDLE handle;
    OS_FileProps props;
    b32 is_valid;
};

#else
struct OS_File
{
    s32 handle;
    OS_FileProps props;
    b32 is_valid;
};
#endif

b32         os_file_exists(char *file_path);
OS_File     os_file_open(char *file_path, OS_FileProps props);
OS_File     os_file_create_and_open(char *file_path, OS_FileProps props);
b32         os_file_create(char *file_path);
b32         os_file_create_directory(char *dir_path);
void        os_file_close(OS_File *file);

s64         os_file_get_size(OS_File *file);
s64         os_file_get_length_to_char(OS_File *file, char c);

String      os_file_read_to_string(Arena *arena, OS_File *file);
String      os_file_read_line(Arena *arena, OS_File *file);
StringArray os_file_read_lines(Arena *arena, OS_File *file);

s64 os_file_get_pos(OS_File *file);
void os_file_set_pos(OS_File *file, s64 pos);
void os_file_reset_pos(OS_File *file);

OS_File os_file_nil();
OS_File os_file_open_readonly(char *file_path);
OS_File os_file_open_writeonly(char *file_path);
OS_File os_file_open_readwrite(char *file_path);
OS_File os_file_create_and_open(char *file_path, OS_FileProps props);

inline s32 os_file_write(OS_File *file, void *data, s32 size);
inline s32 os_file_write_u8(OS_File* file, u8 x);
inline s32 os_file_write_u16(OS_File* file, u16 x);
inline s32 os_file_write_u32(OS_File* file, u32 x);
inline s32 os_file_write_f32(OS_File* file, f32 x);
inline s32 os_file_write_str(OS_File* file, String str);
inline s32 os_file_write_u32_at_index(OS_File* file, u32 x, u32 index);

String      os_get_current_directory();
StringArray os_get_files_in_directory(Arena *arena, String directory);
StringArray os_get_files_by_extensions(Arena *arena, String directory, StringArray extensions);

///////////////////////////////////////
// Paths
///////////////////////////////////////

String os_path_get_extension(Arena *arena, String file_path);

#if OS == OS_WINDOWS

static u32 _map_props_to_access(OS_FileProps props)
{
    u32 access = 0x0;

    if(BIT_CHECK(props, OS_READABLE) && BIT_CHECK(props, OS_WRITABLE))
        access = GENERIC_READ | GENERIC_WRITE;
    else if(BIT_CHECK(props, OS_WRITABLE))
        access = GENERIC_WRITE;
    else if(BIT_CHECK(props, OS_READABLE))
        access = GENERIC_READ;

    return access;
}

OS_File os_file_open(char *file_path, OS_FileProps props)
{
    u32 access = _map_props_to_access(props);
    if(access == 0x0)
        return os_file_nil();

    HANDLE handle;
    handle = CreateFile(file_path, access, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    OS_File file = {
        .handle = (void *)handle,
        .props = props
    };

    file.is_valid = (handle != INVALID_HANDLE);
    return file;
}

OS_File os_file_create_and_open(char *file_path, OS_FileProps props)
{
    u32 access = _map_props_to_access(props);

    HANDLE handle;
    handle = CreateFile(file_path, access, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    OS_File file = {
        .handle = (void *)handle,
        .props = props
    };

    file.is_valid = (handle != INVALID_HANDLE);
    return file;
}

b32 os_file_create(char *file_path)
{
    HANDLE handle;
    handle = CreateFile(file_path, NULL, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    b32 is_valid = (handle != INVALID_HANDLE);

    if(is_valid) CloseHandle(handle);
    return is_valid;
}

b32 os_file_create_directory(char *dir_path)
{
    s32 ret = CreateDirectory(dir_path, NULL);
    return (ret != 0);
}

b32 os_file_exists(char *file_path)
{
    DWORD dwAttrib = GetFileAttributes(file_path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

void os_file_close(OS_File *file)
{
    if(file->handle)
    {
        CloseHandle(file->handle);
    }
}

s64 os_file_get_size(OS_File *file)
{
    if(!file->handle)
        return 0;

    LARGE_INTEGER size;
    if(GetFileSizeEx(file->handle, &size) == FALSE)
        return 0;

    return (s64)(size.QuadPart);
}

s64 os_file_get_pos(OS_File *file)
{
    LARGE_INTEGER dist;
    LARGE_INTEGER fp;

    dist.QuadPart = 0;
    SetFilePointerEx(file->handle, dist, &fp, FILE_CURRENT);

    return (s64)(fp.QuadPart);
}

void os_file_set_pos(OS_File *file, s64 pos)
{
    SetFilePointer(file->handle, pos, NULL, FILE_BEGIN);
    return;
}

s32 os_file_read_char(OS_File *file)
{
    s32 c = 0;
    BOOL result = ReadFile(file->handle, &c, 1, NULL, NULL);
    return result ? c : -1;
}

String os_file_read_to_string(Arena *arena, OS_File *file)
{
    s64 size = os_file_get_size(file);

    String ret = {0};

    if(size == 0)
        return ret;

    ret.data = PUSH_ARRAY(arena, u8, size);

    DWORD bytes_read;
    BOOL result = ReadFile(file->handle, ret.data, size, &bytes_read, NULL);

    ret.len = (u64)(result ? bytes_read : 0);

    return ret;
}

s32 os_file_write(OS_File *file, void *data, s32 size)
{
    DWORD dwSize = (DWORD)size;
    DWORD bytes_written;
    BOOL result;

    result = WriteFile(file->handle, data, dwSize, &bytes_written, NULL);
    return (s32)bytes_written;
}

StringArray os_get_files_in_directory(Arena *arena, String directory)
{
    String search_pattern = string_concat(arena, 2, directory, S("\\*"));
    char *search_pattern_cstr = string_to_cstr(arena, search_pattern);

    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(search_pattern_cstr, &find_data);
    
    if(handle == INVALID_HANDLE_VALUE)
        return string_array_nil();

    StringList sl = string_list_create(arena);

    for(;;)
    {
        b32 is_file = !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        if(is_file)
        {
            String file_name = STR(find_data.cFileName);
            string_list_add(&sl, file_name);
        }

        if(!FileNextFileA(handle, &find_data))
            break;
    };

    return string_list_to_array(sl);
}

String os_get_current_directory()
{
    TCHAR Buffer[MAX_PATH];
    DWORD dwRet;
    dwRet = GetCurrentDirectory(MAX_PATH, Buffer);

    if(dwRet == 0)
    {
        loge("GetCurrentDirectory failed (%d)", GetLastError());
        return string_nil();
    }

    if(dwRet > BUFSIZE)
    {
        loge("Buffer too small; needed %d characters", dwRet);
        return string_nil();
    }

    return STR(Buffer);
}

#else

static s32 _map_props_to_access(OS_FileProps props)
{
    s32 access = -1;

    if(BIT_CHECK(props, OS_READABLE) && BIT_CHECK(props, OS_WRITABLE))
        access = O_RDWR;
    else if(BIT_CHECK(props, OS_WRITABLE))
        access = O_WRONLY;
    else if(BIT_CHECK(props, OS_READABLE))
        access = O_RDONLY;

    return access;
}

OS_File os_file_open(char *file_path, OS_FileProps props)
{
    s32 access = _map_props_to_access(props);

    if(access == -1)
        return os_file_nil();

    s32 fd = open(file_path, access);

    OS_File file = {
        .handle = fd,
        .props = props
    };

    file.is_valid = (fd > -1);
    return file;
}

OS_File os_file_create_and_open(char *file_path, OS_FileProps props)
{
    s32 access = _map_props_to_access(props);
    if(access == -1) access = O_CREAT | O_TRUNC;

    s32 fd = open(file_path, access, 0644);

    OS_File file = {
        .handle = fd,
        .props = props
    };

    file.is_valid = (fd > -1);
    return file;
}

b32 os_file_create(char *file_path)
{
    s32 access = O_CREAT | O_TRUNC;
    s32 fd = open(file_path, access, 0644);
    b32 is_valid = (fd > -1);
    if(is_valid) close(fd);
    return is_valid;
}

b32 os_file_create_directory(char *dir_path)
{
    s32 err = mkdir(dir_path, 0644);
    return (err == 0);
}

b32 os_file_exists(char *file_path)
{
     if(access(file_path, F_OK)) return true;
     else return false;
}

void os_file_close(OS_File *file)
{
    close(file->handle);
}

s64 os_file_get_size(OS_File *file)
{
    s64 pos = os_file_get_pos(file);
    os_file_reset_pos(file);
    s64 size = lseek(file->handle, 0, SEEK_END);
    os_file_set_pos(file, pos);

    return size;
}

s64 os_file_get_pos(OS_File *file)
{
    if(!file->handle)
        return 0;

    return (s64)lseek(file->handle, 0, SEEK_CUR);
}

void os_file_set_pos(OS_File *file, s64 pos)
{
    lseek(file->handle, pos, SEEK_SET);
    return;
}

s32 os_file_read_char(OS_File *file)
{
    u8 c = '\0';
    s32 bytes_read = read(file->handle, &c, sizeof(u8));
    return c;
}

String os_file_read_to_string(Arena *arena, OS_File *file)
{
    s64 size = os_file_get_size(file);

    String ret = {0};
    ret.data = (u8 *)PUSH_ARRAY(arena, u8, size);
    ret.len = (u64)size;

    s32 c;
    u64 i = 0;

    for(;;)
    {
        c = os_file_read_char(file);
        if(c == 0)
            break;

        ret.data[i++] = c;
    }

    ret.len = (u64)i;

    return ret;
}

s32 os_file_write(OS_File *file, void *data, s32 size)
{
    return write(file->handle, data, size);
}

String os_get_current_directory()
{
    char cwd[1024] = {0};
    if(!getcwd(cwd, sizeof(cwd)))
        return string_nil();

    String str = STR(cwd);
    return str;
}

StringArray os_get_files_in_directory(Arena *arena, String directory)
{
    char *directory_cstr = string_to_cstr(arena, directory);
    DIR* dir = opendir(directory_cstr);
    if(!dir) return string_array_nil();;

    struct dirent *entry = NULL;

    StringList sl = string_list_create(arena);

    for(;;)
    {
        entry = readdir(dir);
        if(!entry) break;

        b32 is_file = (entry->d_type != DT_DIR);
        if(is_file)
        {
            String file_name = STR(entry->d_name);
            string_list_add(&sl, file_name);
        }
    };

    return string_list_to_array(sl);
}

#endif

///////////////////////////////////////
// File Helpers
///////////////////////////////////////

OS_File os_file_nil()
{
    OS_File file = {0};
    return file;
}

OS_File os_file_open_readonly(char *file_path)
{
    return os_file_open(file_path, OS_READABLE);
}

OS_File os_file_open_writeonly(char *file_path)
{
    return os_file_open(file_path, OS_WRITABLE);
}

OS_File os_file_open_readwrite(char *file_path)
{
    return os_file_open(file_path, (OS_FileProps)(OS_READABLE | OS_WRITABLE));
}

void os_file_reset_pos(OS_File *file)
{
    os_file_set_pos(file, 0);
}

s64 os_file_get_length_to_char(OS_File *file, char c)
{
    s64 pos = os_file_get_pos(file);
    s64 len = 0;

    for(;;)
    {
        s32 n = os_file_read_char(file);

        if(n == 0 || n == -1 || n == EOF) { len = 0; break; }
        if(n == c) break;
            
        len++;
    }

    os_file_set_pos(file, pos);

    return len;
}

String os_file_read_line(Arena *arena, OS_File *file)
{
    String str = {0};

    s64 len = os_file_get_length_to_char(file, '\n');
    if(len == 0) return str;

    str.data = (u8 *)PUSH_ARRAY(arena, u8, len);

    for(s64 i = 0; i < len; ++i)
    {
        s32 c = os_file_read_char(file);
        str.data[str.len++] = (u8)c;
    }

    os_file_read_char(file); // read one more to discard newline

    return str;
}

StringArray os_file_read_lines(Arena *arena, OS_File *file)
{
    String str = os_file_read_to_string(arena, file);
    StringArray sa = string_split(arena, str, S("\n"));

    return sa;
}

inline s32 os_file_write_u8(OS_File* file, u8 x)   { return os_file_write(file, &x, sizeof(u8)); }
inline s32 os_file_write_u16(OS_File* file, u16 x) { return os_file_write(file, &x, sizeof(u16)); }
inline s32 os_file_write_u32(OS_File* file, u32 x) { return os_file_write(file, &x, sizeof(u32)); }
inline s32 os_file_write_f32(OS_File* file, f32 x) { return os_file_write(file, &x, sizeof(f32)); }
inline s32 os_file_write_str(OS_File* file, String str) { return os_file_write(file, str.data, str.len); }
inline s32 os_file_write_u32_at_index(OS_File* file, u32 x, u32 index)
{
    s64 pos = os_file_get_pos(file);
    os_file_set_pos(file, index);
    s32 ret = os_file_write_u32(file, x);
    os_file_set_pos(file, pos);
    return ret;
}

StringArray os_get_files_by_extensions(Arena *arena, String directory, StringArray extensions)
{
    StringArray files = os_get_files_in_directory(arena, directory);

    StringList filtered = string_list_create(arena);

    for(int i = 0; i < files.count; ++i)
    {
        String file = files.items[i];
        string_to_lower(&file);

        for(int j = 0; j < extensions.count; ++j)
        {
            String ext = extensions.items[j];
            if(ext.len == 0) continue;

            if(string_ends_with(file, ext))
            {
                // add to new list
                string_list_add(&filtered, file);
            }
        }
    }

    return string_list_to_array(filtered);
}

///////////////////////////////////////
// File Paths
///////////////////////////////////////

String os_path_get_extension(Arena *arena, String file_path)
{
    char ext[256] = {0};
    u32 ext_i = 0;

    for(int i = file_path.len - 1; i >= 0; --i)
    {
        char c = file_path.data[i];
        if(c == '.') break;

        ext[ext_i++] = c;
        if(ext_i >= 256)
            break;
    }

    String extension;
    extension.len = ext_i;
    extension.data = (u8 *)PUSH_ARRAY(arena, u8, ext_i);
    MemoryCopy(extension.data, &file_path.data[file_path.len - ext_i], ext_i);

    return extension;
}



#ifdef __cplusplus
}
#endif

//////////////////////////////
// Program-specific types
//////////////////////////////

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
        case TRANSFORM_TYPE_NONE:           return "None";
        case TRANSFORM_TYPE_BLACKOUT:       return "Black Out";
        case TRANSFORM_TYPE_BLUR:           return "Blur";
        case TRANSFORM_TYPE_PIXELATE:       return "Pixelate";
        case TRANSFORM_TYPE_SCRAMBLE:       return "Scramble";
        case TRANSFORM_TYPE_SCRAMBLE_FIXED: return "Scramble (Fixed Seed)";
        case TRANSFORM_TYPE_TEXTURE:        return "Texture";
        default:                            return "Unknown";
    }
}

typedef struct
{
    u16 x;
    u16 y;
} PointU16;

typedef struct
{
    s32 x;
    s32 y;
} Point;

typedef struct
{
    s32 x;
    s32 y;
    s32 w;
    s32 h;
    s32 confidence;
    Point landmarks[5];
    b32 interpolated;
} Box;

typedef struct
{
    u32 box_count;
    Box* boxes;
} BoxList;

typedef struct
{
    u8 *data;
    s32 w;
    s32 h;
    s32 n; // channels
    s32 step; // number of bytes to advance to next row
    s32 rotation; // 0, 90, 180, 270

    f32 scale_x;
    f32 scale_y;

    // used for sub-image thread processing
    u8 *detect_buffer;
    u8 subx; // position in larger image
    u8 suby; // position in larger image
    void* arena;
    b32 scaled; // determine if image was scaled
    u32 frame_number; // used for video reconstruction
    u8* result;
} Image;

typedef struct {
    u16 w;
    u16 h;
    u32 frame_count;        // number of frames currently in the buffer
    s64 total_frame_count;  // total number of frames in the video
    u32 frames_processed;   // used during encoding
    f32 fps;
    u8* data;               // RGB buffer for current chunk
    s64* pts_buffer;        // PTS for each frame in current chunk
    s32 rotation;
    s32 data_max_frames;    // max frames in current buffer
    b32 decode_complete;
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
    String filename;
    Image image;
} InputFile;

typedef struct
{
    AssetType asset_type;
    DetectClass classification;

    Transform transforms[10];
    s32 transform_count;

    String input_file_text;
    String input_directory;
    String output_file_path;

    InputFile input_files[100];

    s32 input_file_count;
    s32 thread_count;

    u16 confidence_threshold;
    f32 nms_iou_threshold;

    b32 has_texture;
    char texture_image_path[256];

    f32 block_scale; // 0.0 - 1.0
    f32 blur_strength; // 0.0 - 1.0
    f32 box_padding_pct; // 0.0 - 1.0
    f32 frame_smoothing_window; // 0.0 - 1.0

    u32 scaled_size_image;
    u32 scaled_size_video;

    u64 max_buffer_size;

    String bbx_output;
    b32 has_bbx_output;

    b32 no_encoding;
    b32 no_scale;
    b32 no_rotate;
    b32 debug;
    b32 verbose;

} ProgramSettings;

typedef struct
{
    b32 success;
    s32 err_code;
} FunctionResult;

#define MAX_ARENAS 64

#define BBX_VERSION 1
#define BBX_FRAME_COUNT_OFFSET 12

extern ProgramSettings settings;

#if 0 //PLATFORM == PLATFORM_WINDOWS
extern HANDLE *threads;
#else
extern pthread_t *threads;
#endif

extern Timer timer;
extern Arena* thread_arenas[MAX_ARENAS];
extern Image texture_image;

