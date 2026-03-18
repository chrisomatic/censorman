#pragma once

///////////////////////////////////////
// SECTIONS:
// - OS Defines
// - Includes
// - Memory
// - System
// - Time
// - File
// - Log
// - Threads
// - Socket
// - Utility
///////////////////////////////////////

///////////////////////////////////////
// OS Defines
///////////////////////////////////////

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

///////////////////////////////////////
// Includes
///////////////////////////////////////

#if OS == OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <profileapi.h>
#include <handleapi.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#pragma comment( lib, "wsock32.lib" )
#else
#include <unistd.h> // for usleep
#include <fcntl.h>
#include <time.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

///////////////////////////////////////
// Memory
///////////////////////////////////////

void *os_reserve(u64 size);
b32  os_commit(void *ptr, u64 size);
void os_decommit(void *ptr, u64 size);
void os_release(void *ptr, u64 size);
void *os_reserve_large(u64 size);
b32  os_commit_large(void *ptr, u64 size);

///////////////////////////////////////
// System
///////////////////////////////////////

typedef struct
{
    b32 initialized;
    u32 logical_processor_count;
    u64 page_size;
} OS_SystemInfo;

extern OS_SystemInfo os_system_info;

void os_system_init();
String os_system_get_executable_path(Arena *arena);

///////////////////////////////////////
// Time
///////////////////////////////////////

typedef struct
{
    s32 year;
    s32 month;
    s32 day_of_week;
    s32 day;
    s32 hour;
    s32 minute;
    s32 second;
    s32 millisecond;
} OS_TimeData;

OS_TimeData os_time_data_get();

typedef struct
{
    f32  fps;
    f32  spf;
    f64 time_start;
    f64 time_last;
    f64 frame_fps;
} Timer;

struct
{
    b32 monotonic;
    u64  frequency;
    u64  offset;
} _timer;

#if OS == OS_WINDOWS
void usleep(s64 usec);
#endif

u64  os_time_value_u64(void);
void os_time_init(void);
f64  os_time_get();
void os_time_begin(Timer* timer);
void os_time_set_fps(Timer* timer, f32 fps);
void os_time_wait_for_frame(Timer* timer);
f64  os_time_get_elapsed(Timer* timer);
f64  os_time_get_prior_frame_fps(Timer* timer);
void os_time_delay_us(u64 us);

///////////////////////////////////////
// Entropy
///////////////////////////////////////

b32 os_entropy(u8 *data, u64 len);

///////////////////////////////////////
// Stopwatch (simple profiling)
///////////////////////////////////////

#define STOPWATCH_MAX_ENTRIES 32
typedef struct
{
    u32 hash;
    String label;
    u64 start_value;
    f64 total_seconds;
} StopwatchEntry;

typedef struct
{
    StopwatchEntry entries[STOPWATCH_MAX_ENTRIES];
    u8 entry_count;
} Stopwatch;

Stopwatch stopwatch_create();

void stopwatch_reset(Stopwatch *stopwatch);
void stopwatch_begin(Stopwatch *stopwatch, String entry_str);
void stopwatch_end(Stopwatch *stopwatch, String entry_str);
void stopwatch_print(Stopwatch *stopwatch);

///////////////////////////////////////
// Files
///////////////////////////////////////

typedef struct OS_File OS_File;

typedef enum
{
    OS_READABLE = (1 << 0),
    OS_WRITABLE = (1 << 1),
} OS_FileProps;

OS_File     os_file_open(char *file_path, OS_FileProps props);
OS_File     os_file_open_readonly(char *file_path);
OS_File     os_file_open_writeonly(char *file_path);
OS_File     os_file_open_readwrite(char *file_path);
void        os_file_close(OS_File file);

OS_File     os_file_create_and_open(char *file_path, OS_FileProps props);
b32         os_file_create(char *file_path);
b32         os_file_create_directory(char *dir_path);
b32         os_file_delete(char *file_path);
b32         os_file_delete_directory(char *dir_path);

b32         os_file_exists(char *file_path);
b32         os_file_is_valid(OS_File file);
s64         os_file_get_size(OS_File file);
s64         os_file_get_length_to_char(OS_File file, char c);

String      os_file_read_to_string(Arena *arena, OS_File file);
String      os_file_read_line(Arena *arena, OS_File file);
StringArray os_file_read_lines(Arena *arena, OS_File file);

inline s32  os_file_write(OS_File file, void *data, s32 size);
inline s32  os_file_write_u8(OS_File file, u8 x);
inline s32  os_file_write_u16(OS_File file, u16 x);
inline s32  os_file_write_u32(OS_File file, u32 x);
inline s32  os_file_write_f32(OS_File file, f32 x);
inline s32  os_file_write_str(OS_File file, String str);
inline s32  os_file_write_u32_at_index(OS_File file, u32 x, u32 index);

s64  os_file_get_pos(OS_File file);
void os_file_set_pos(OS_File file, s64 pos);
void os_file_reset_pos(OS_File file);

OS_File os_file_nil();

String      os_get_current_directory();
StringArray os_get_files_in_directory(Arena *arena, String directory);
StringArray os_get_files_by_extensions(Arena *arena, String directory, StringArray extensions);

///////////////////////////////////////
// Paths
///////////////////////////////////////

b32    os_path_is_directory(String path);
String os_path_get_extension(String path);
String os_path_get_directory(String path);
String os_path_get_file(String path);

///////////////////////////////////////
// Logs
///////////////////////////////////////

#define logi(...) os_log(LOG_LEVEL_INFO,    __FILE__, __LINE__, __VA_ARGS__);
#define logw(...) os_log(LOG_LEVEL_WARN,    __FILE__, __LINE__, __VA_ARGS__);
#define loge(...) os_log(LOG_LEVEL_ERROR,   __FILE__, __LINE__, __VA_ARGS__);
#define logd(...) os_log(LOG_LEVEL_DEBUG,   __FILE__, __LINE__, __VA_ARGS__);
#define logv(...) os_log(LOG_LEVEL_VERBOSE, __FILE__, __LINE__, __VA_ARGS__);

typedef enum
{
    LOG_LEVEL_QUIET = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE,
} LogLevel;

void os_log(LogLevel level, const char* file, s32 line, const char* fmt, ...);

LogLevel os_get_log_level(void);
void os_set_log_level(LogLevel level);

void os_printf(const char* fmt, ...);
void os_vprintf(const char* fmt, va_list args);
s32  os_print_raw(const char* msg, s32 msg_len);

///////////////////////////////////////
// Threads
///////////////////////////////////////

#define NARROW if(thread_index == 0)

typedef struct Thread Thread;
typedef struct Barrier Barrier;

#if OS == OS_WINDOWS

#if defined(_MSC_VER)
  #define THREAD_LOCAL __declspec(thread)
#elif defined(__MINGW32__) || defined(__MINGW64__)
  #define THREAD_LOCAL __thread
#endif

struct Thread
{
    HANDLE handle;
    DWORD  id;
};
struct Barrier
{
    LPSYNCHRONIZATION_BARRIER barrier;
};
typedef struct
{
    CRITICAL_SECTION handle;
} Mutex;

typedef long unsigned int (*ThreadFunc)(void *);
#else
#define THREAD_LOCAL __thread
struct Thread
{
    pthread_t handle;
};
struct Barrier
{
    pthread_barrier_t barrier;
};
typedef struct
{
    pthread_mutex_t handle;
} Mutex;

typedef void* (*ThreadFunc)(void *);
#endif

typedef struct
{
    s64 index;
    s64 count;
} ThreadContext;

typedef struct
{
    s64 min;
    s64 max;
} ThreadValuesRange;

Thread thread_launch(ThreadFunc func, void *arg);
void   thread_join(Thread thread);
void   thread_close(Thread thread);

void   thread_join_many(Thread *threads, s64 thread_count);
void   thread_close_many(Thread *threads, s64 thread_count);

ThreadValuesRange thread_range(u64 values_count);

Barrier barrier_create(s64 thread_count);
b32     barrier_sync(Barrier *barrier);
void barrier_destroy(Barrier *barrier);

Mutex mutex_create(void);
void  mutex_lock(Mutex *m);
void  mutex_unlock(Mutex *m);
void  mutex_destroy(Mutex *m);

///////////////////////////////////////
// Socket
///////////////////////////////////////

#define MAX_PACKET_SIZE 32768

typedef struct
{
    u8 a;
    u8 b;
    u8 c;
    u8 d;
    u16 port;
} Address;

b32 socket_initialize();
b32 socket_create(s32* socket_handle);
b32 socket_bind(s32 socket_handle, Address* address, u16 port);
s32 socket_sendto(s32 socket_handle, Address* address, u8* pkt, u32 pkt_size);
s32 socket_recvfrom(s32 socket_handle, Address* address, u8* pkt);

void socket_close(s32 socket_handle);
void socket_shutdown();

///////////////////////////////////////
// Utility
///////////////////////////////////////

void os_wait_for_return_key(void);
