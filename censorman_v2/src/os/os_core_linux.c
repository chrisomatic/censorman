
///////////////////////////////////////
// Memory
///////////////////////////////////////

void *os_reserve(u64 size)
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED)
    {
        result = 0;
    }
    return result;
}

b32 os_commit(void *ptr, u64 size)
{
    mprotect(ptr, size, PROT_READ|PROT_WRITE);
    return 1;
}

void os_decommit(void *ptr, u64 size)
{
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
}

void os_release(void *ptr, u64 size)
{
    munmap(ptr, size);
}

void *os_reserve_large(u64 size)
{
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_HUGETLB, -1, 0);
    if(result == MAP_FAILED)
    {
        result = 0;
    }
    return result;
}

b32 os_commit_large(void *ptr, u64 size)
{
    mprotect(ptr, size, PROT_READ|PROT_WRITE);
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

    info->logical_processor_count = (u32)get_nprocs();
    info->page_size = (u64)getpagesize();

    info->initialized = true;
}

String os_system_get_executable_path(Arena *arena)
{
    char path_buf[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", path_buf, PATH_MAX);

    String path;
    path.len = (u64)len;
    path.data = PUSH_ARRAY(arena, char, len);
    MemoryCopy(path.data,path_buf,len*sizeof(char));

    return path;
}


///////////////////////////////////////
// Time
///////////////////////////////////////

OS_TimeData os_time_data_get()
{
    OS_TimeData time_data = {0};

    time_t t = time(NULL);
    struct tm* _time = localtime(&t);

    time_data.hour   = _time->tm_hour;
    time_data.minute = _time->tm_min;
    time_data.second = _time->tm_sec;

    time_data.year   = _time->tm_year;
    time_data.month  = _time->tm_mon;
    time_data.day_of_week = _time->tm_wday;
    time_data.day    = _time->tm_mday;
    time_data.hour   = _time->tm_hour;
    time_data.minute = _time->tm_min;
    time_data.second = _time->tm_sec;
    time_data.millisecond = 0;

    return time_data;
}

u64 os_time_value_u64(void)
{
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
}

void os_time_init(void)
{
    time_t t;
    srand((unsigned) time(&t));
    
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
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000*us;
    nanosleep(&ts, NULL);
}

///////////////////////////////////////
// Files
///////////////////////////////////////

#define OS_PATH_SLASH '/'

struct OS_File
{
    s32 handle;
    OS_FileProps props;
    b32 is_valid;
};

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

b32 os_file_delete(char *file_path)
{
    s32 status = unlink(file_path);
    return (status == 0);
}

b32 os_file_delete_directory(char *dir_path)
{
    int status = rmdir(dir_path);
    return (status == 0);
}

b32 os_file_exists(char *file_path)
{
     if(access(file_path, F_OK)) return true;
     else return false;
}

void os_file_close(OS_File file)
{
    close(file.handle);
}

s64 os_file_get_size(OS_File file)
{
    s64 pos = os_file_get_pos(file);
    os_file_reset_pos(file);
    s64 size = lseek(file.handle, 0, SEEK_END);
    os_file_set_pos(file, pos);

    return size;
}

s64 os_file_get_pos(OS_File file)
{
    if(!file.handle)
        return 0;

    return (s64)lseek(file.handle, 0, SEEK_CUR);
}

void os_file_set_pos(OS_File file, s64 pos)
{
    lseek(file.handle, pos, SEEK_SET);
    return;
}

s32 os_file_read_char(OS_File file)
{
    u8 c = '\0';
    s32 bytes_read = read(file.handle, &c, sizeof(u8));
    return c;
}

String os_file_read_to_string(Arena *arena, OS_File file)
{
    s64 size = os_file_get_size(file);

    String ret = {0};
    ret.data = PUSH_ARRAY(arena, u8, size);
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

s32 os_file_write(OS_File file, void *data, s32 size)
{
    return write(file.handle, data, size);
}

String os_get_current_directory()
{
    u8 cwd[1024] = {0};
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

///////////////////////////////////////
// Paths
///////////////////////////////////////

b32 os_path_is_directory(String path)
{
    Temp scratch = scratch_begin();
    char *path_cstr = string_to_cstr(scratch.arena, path);

    struct stat path_stat;
    s32 res = stat(path_cstr, &path_stat);
    scratch_end(scratch);

    if(res != 0) return false;

    return S_ISDIR(path_stat.st_mode);
}

///////////////////////////////////////
// Log
///////////////////////////////////////

const char* log_level_strings[] = {
  "INFO", "WARN", "ERROR", "DEBUG", "VERB"
};

const char* log_level_colors[] = {
  "\x1b[94m", "\x1b[33m", "\x1b[31m", "\x1b[35m", "\x1b[36m"
};

void os_log(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    if(level > os_get_log_level())
        return;

    OS_TimeData td = os_time_data_get();

    va_list ap;
    va_start(ap, fmt);

    fprintf(stdout, "%d:%d:%d %s%-5s\x1b[0m \x1b[90m%s:%-4d:\x1b[0m ", td.hour, td.minute, td.second, log_level_colors[level], log_level_strings[level], file, line);

    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    fflush(stdout);
    va_end(ap);
}

void os_printf(const char* fmt, ...)
{
    va_list va;

    Temp scratch = scratch_begin();
    va_start(va, fmt);

    int count = stbsp_vsnprintf(NULL, 0, fmt, va);
    char *buf = PUSH_ARRAY(scratch.arena, char, count);
    stbsp_vsnprintf(buf,count,fmt, va);
    va_end(va);

    os_print_raw(buf, count);

    scratch_end(scratch);
}

int os_print_raw(const char* msg, s32 msg_len)
{
    return write(STDOUT_FILENO, msg, msg_len);
}

///////////////////////////////////////
// Socket
///////////////////////////////////////

b32 socket_initialize()
{
    return true;
}

void socket_shutdown()
{
    return;
}

b32 socket_create(s32* socket_handle)
{
    *socket_handle = socket(AF_INET, SOCK_DGRAM, 0);

    if(*socket_handle <= 0 )
    {
        s32 errorcode = *socket_handle;
        loge("Failed to create socket. Error: %d\n", errorcode);
        return false;
    }

    return true;
}

void socket_close(s32 socket_handle)
{
    close(socket_handle);
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
