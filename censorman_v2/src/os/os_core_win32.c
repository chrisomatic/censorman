///////////////////////////////////////
// Memory
///////////////////////////////////////

void *os_reserve(u64 size)
{
    void *result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    return result;
}

b32 os_commit(void *ptr, u64 size)
{
    b32 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);
    return result;
}

void os_decommit(void *ptr, u64 size)
{
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

void os_release(void *ptr, u64 size)
{
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void *os_reserve_large(u64 size)
{
  void *result = VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT|MEM_LARGE_PAGES, PAGE_READWRITE);
  return result;
}

b32 os_commit_large(void *ptr, u64 size)
{
  return 1;
}

///////////////////////////////////////
// System
///////////////////////////////////////

OS_SystemInfo os_system_info = {0};

void os_system_init()
{
    OS_SystemInfo *info = &os_system_info;

    if(info->initialized) return;

    SYSTEM_INFO sysinfo = {0};
    GetSystemInfo(&sysinfo);

    info->logical_processor_count = (u64)sysinfo.dwNumberOfProcessors;
    info->page_size               = sysinfo.dwPageSize;

    info->initialized = true;
}

String os_system_get_executable_path(Arena *arena)
{
    char path_buf[MAX_PATH] = {0};
    DWORD len = GetModuleFileName(NULL, path_buf, MAX_PATH);

    String path;
    path.len = (u64)len;
    path.data = PUSH_ARRAY(arena, char, len);
    MemoryCopy(path.data,path_buf,len*sizeof(char));

    s64 idx = string_get_first_index(path,"\\", false);

    return path;
}


///////////////////////////////////////
// Time
///////////////////////////////////////

OS_TimeData os_time_data_get()
{
    OS_TimeData time_data = {0};

    SYSTEMTIME lt;
    GetLocalTime(&lt);

    time_data.year   = lt.wYear;
    time_data.month  = lt.wMonth;
    time_data.day_of_week = lt.wDayOfWeek;
    time_data.day    = lt.wDay;
    time_data.hour   = lt.wHour;
    time_data.minute = lt.wMinute;
    time_data.second = lt.wSecond;
    time_data.millisecond = lt.wMilliseconds;

    return time_data;
}


void usleep(__int64 usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}

u64 os_time_value_u64(void)
{
    u64 counter;
    QueryPerformanceCounter((LARGE_INTEGER*)&counter);
    return counter;
}

void os_time_init(void)
{
    time_t t;
    srand((unsigned) time(&t));
    
    u64 freq;
    QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
    _timer.monotonic = false;
    _timer.frequency = freq;
    _timer.offset = os_time_value_u64();
}

f64 os_time_get()
{
    return (f64) (os_time_value_u64() - _timer.offset) / (f64)_timer.frequency;
}

void os_time_begin(Timer* timer)
{
    timer->time_start = os_time_get();
    timer->time_last = timer->time_start;
    timer->frame_fps = 0.0f;
}

void os_time_set_fps(Timer* timer, f32 fps)
{
    timer->fps = fps;
    timer->spf = 1.0f / fps;
}

void os_time_wait_for_frame(Timer* timer)
{
    f64 now;
    for(;;)
    {
        now = os_time_get();
        if(now >= timer->time_last + timer->spf)
            break;
    }

    timer->frame_fps = 1.0f / (now - timer->time_last);
    timer->time_last = now;
}

f64 os_time_get_elapsed(Timer* timer)
{
    f64 time_curr = os_time_get();
    return time_curr - timer->time_start;
}

f64 os_time_get_prior_frame_fps(Timer* timer)
{
    return timer->frame_fps;
}

void os_time_delay_us(u64 us)
{
    usleep(us);
}

///////////////////////////////////////
// Files
///////////////////////////////////////

#define OS_PATH_SLASH '\\'

struct OS_File
{
    HANDLE handle;
    OS_FileProps props;
    b32 is_valid;
};

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

    file.is_valid = (handle != INVALID_HANDLE_VALUE);
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

    file.is_valid = (handle != INVALID_HANDLE_VALUE);
    return file;
}

b32 os_file_create(char *file_path)
{
    HANDLE handle;
    handle = CreateFileA(file_path, 0, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    b32 is_valid = (handle != INVALID_HANDLE_VALUE);

    if(is_valid) CloseHandle(handle);
    return is_valid;
}

b32 os_file_create_directory(char *dir_path)
{
    s32 ret = CreateDirectory(dir_path, NULL);
    return (ret != 0);
}

b32 os_file_delete(char *file_path)
{
    BOOL deleted = DeleteFileA(file_path);
    return (deleted);
}

b32 os_file_delete_directory(char *dir_path)
{
    BOOL removed = RemoveDirectoryA(dir_path);
    return removed;
}

b32 os_file_exists(char *file_path)
{
    DWORD dwAttrib = GetFileAttributes(file_path);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES);
}

void os_file_close(OS_File file)
{
    if(file.handle)
    {
        CloseHandle(file.handle);
    }
}

s64 os_file_get_size(OS_File file)
{
    if(!file.handle)
        return 0;

    LARGE_INTEGER size;
    if(GetFileSizeEx(file.handle, &size) == FALSE)
        return 0;

    return (s64)(size.QuadPart);
}

s64 os_file_get_pos(OS_File file)
{
    LARGE_INTEGER dist;
    LARGE_INTEGER fp;

    dist.QuadPart = 0;
    SetFilePointerEx(file.handle, dist, &fp, FILE_CURRENT);

    return (s64)(fp.QuadPart);
}

void os_file_set_pos(OS_File file, s64 pos)
{
    SetFilePointer(file.handle, pos, NULL, FILE_BEGIN);
    return;
}

s32 os_file_read_char(OS_File file)
{
    s32 c = 0;
    BOOL result = ReadFile(file.handle, &c, 1, NULL, NULL);
    return result ? c : -1;
}

String os_file_read_to_string(Arena *arena, OS_File file)
{
    s64 size = os_file_get_size(file);

    String ret = {0};

    if(size == 0)
        return ret;

    ret.data = PUSH_ARRAY(arena, u8, size);

    DWORD bytes_read;
    BOOL result = ReadFile(file.handle, ret.data, size, &bytes_read, NULL);

    ret.len = (u64)(result ? bytes_read : 0);

    return ret;
}

s32 os_file_write(OS_File file, void *data, s32 size)
{
    DWORD dwSize = (DWORD)size;
    DWORD bytes_written;
    BOOL result;

    result = WriteFile(file.handle, data, dwSize, &bytes_written, NULL);
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

        if(!FindNextFileA(handle, &find_data))
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

    if(dwRet > MAX_PATH)
    {
        loge("Buffer too small; needed %d characters", dwRet);
        return string_nil();
    }

    return STR(Buffer);
}

///////////////////////////////////////
// Paths
///////////////////////////////////////

b32 os_path_is_directory(String path)
{
    Temp scratch = scratch_begin();
    char *path_cstr = string_to_cstr(scratch.arena, path);
    DWORD dwAttrib = GetFileAttributesA(path_cstr);
    scratch_end(scratch);

    if (dwAttrib == INVALID_FILE_ATTRIBUTES)
        return false;

    // Check if the directory attribute is set
    return (dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

///////////////////////////////////////
// Log
///////////////////////////////////////

const char* log_level_strings[] = {
  "QUIET", "INFO", "WARN", "ERROR", "DEBUG", "VERB"
};

const char* log_level_colors[] = {
  "\x1b[30m", "\x1b[94m", "\x1b[33m", "\x1b[31m", "\x1b[35m", "\x1b[36m"
};

void os_log(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    if(level > os_get_log_level())
        return;

    va_list args;
    va_start(args, fmt);

    int file_len = strlen(file);
    char* file_trunc = (char *)file + file_len - MIN(file_len, 20);

    f64 uptime = os_time_get();

    s32 uptime_min = (s32)(uptime / 60.0);
    s32 uptime_sec = (s32)(uptime);
    s32 uptime_ms  = (s32)((uptime - uptime_sec)*1000.0);

    os_printf("[%02d:%02d.%03d] %-5s...%-20s:%-4d: ", uptime_min, uptime_sec, uptime_ms, log_level_strings[level], file_trunc, line);
    os_vprintf(fmt, args);
    os_print_raw("\n", 1);

    va_end(args);
}

void os_vprintf(const char* fmt, va_list args)
{
#if 1
    Temp scratch = scratch_begin();
    int count = stbsp_vsnprintf(NULL, 0,fmt, args);
    char *buf = PUSH_ARRAY(scratch.arena, char, count+1);
    stbsp_vsnprintf(buf,count+1,fmt, args);
    os_print_raw(buf, count);
    scratch_end(scratch);
#else
    char buf[4096+1] = {0};
    int count = stbsp_vsnprintf(buf,4096+1,fmt, args);
    os_print_raw(buf, count);
#endif
}

void os_printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    os_vprintf(fmt,args);
    va_end(args);
}

static HANDLE __std_handle = NULL;
int os_print_raw(const char* msg, s32 msg_len)
{
    DWORD chars_written = 0;

    if(__std_handle == NULL)
    {
        __std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    }
        
    if(__std_handle == INVALID_HANDLE_VALUE || __std_handle == NULL) return 0;
    if(!WriteConsoleA(__std_handle,msg,msg_len,&chars_written,NULL)) return 0;

    return chars_written;
}

///////////////////////////////////////
// Threads
///////////////////////////////////////

Thread thread_create(ThreadFunc func, void *arg)
{
    Thread thread = {0};
    thread.handle = CreateThread(NULL, 0, func, arg, 0, &thread.id);

    return thread;
}

void thread_join(Thread *thread)
{
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
}

void thread_join_many(u32 thread_count, Thread *threads)
{
    HANDLE handles[256] = {0};
    for(int i = 0; i < thread_count; ++i)
    {
        handles[i] = threads[i].handle;
    }
    WaitForMultipleObjects(thread_count, handles, TRUE, INFINITE);
}

///////////////////////////////////////
// Socket
///////////////////////////////////////

b32 socket_initialize()
{
    WSADATA WsaData;
    return WSAStartup( MAKEWORD(2,2), &WsaData ) == NO_ERROR;
}

void socket_shutdown()
{
    WSACleanup();
}

b32 socket_create(s32* socket_handle)
{
    *socket_handle = socket(AF_INET, SOCK_DGRAM, 0);

    if(*socket_handle <= 0 )
    {
        s32 errorcode = *socket_handle;
        errorcode = WSAGetLastError();
        printf("Failed to create socket. Error: %d\n", errorcode);
        return false;
    }

    return true;
}

void socket_close(s32 socket_handle)
{
    closesocket(socket_handle);
}

b32 socket_bind(s32 socket_handle, Address* address, u16 port)
{
    struct sockaddr_in to = {0};

    to.sin_family = AF_INET;

    if(address == NULL)
    {
        to.sin_addr.s_addr = htonl(INADDR_ANY); // server
    }
    else
    {
        u32 address_uint32_t = (address->a << 24) | (address->b << 16) | (address->c << 8) | (address->d);
        to.sin_addr.s_addr = htonl(address_uint32_t);
    }

    to.sin_port = htons(port);

    if (bind(socket_handle,(const struct sockaddr*) &to, sizeof(struct sockaddr_in)) < 0)
    {
        perror("Failed to bind socket.\n");
        return false;
    }

    return true;
}

s32 socket_sendto(s32 socket_handle, Address* address, u8* pkt, u32 pkt_size)
{
    struct sockaddr_in to = {0};

    u32 address_uint32_t = (address->a << 24) | (address->b << 16) | (address->c << 8) | (address->d);

    to.sin_family      = AF_INET;
    to.sin_addr.s_addr = htonl(address_uint32_t);
    to.sin_port        = htons(address->port);

    s32 sent_bytes = sendto(socket_handle,(const u8*)pkt, pkt_size, 0, (struct sockaddr*)&to, sizeof(struct sockaddr_in));

    if (sent_bytes != pkt_size)
    {
        perror("Failed to send packet.\n");
        return 0;
    }

    return sent_bytes;
}

s32 socket_recvfrom(s32 socket_handle, Address* address, u8* pkt)
{
    u8 packet_data[MAX_PACKET_SIZE] = {0};

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    s32 recv_bytes = recvfrom(socket_handle, (u8*)&packet_data, MAX_PACKET_SIZE, 0, (struct sockaddr*)&from, &from_len);

    memcpy(pkt,packet_data,recv_bytes);

    address->a = (u8)(from.sin_addr.s_addr >> 0);
    address->b = (u8)(from.sin_addr.s_addr >> 8);
    address->c = (u8)(from.sin_addr.s_addr >> 16);
    address->d = (u8)(from.sin_addr.s_addr >> 24);
    address->port = ntohs(from.sin_port);

    if (recv_bytes < 0 )
    {
        perror("Failed to receive packet.\n" );
        return 0;
    }

    return recv_bytes;
}

///////////////////////////////////////
// Utility
///////////////////////////////////////

void os_wait_for_return_key()
{
    logi("Waiting for return key...");

    for(;;)
    {
        if(GetAsyncKeyState(VK_RETURN) & 0x8000)
            break;

        os_time_delay_us(10*1000);
    }
}
