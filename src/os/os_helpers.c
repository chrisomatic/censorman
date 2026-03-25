
// The following functions are OS-agnostic,
// derived from the core OS functions

///////////////////////////////////////
// Utility
///////////////////////////////////////

#define DELAY_MS(s) os_time_delay_us((s)*1000)

LogLevel      s_log_level = LOG_LEVEL_DEBUG;
THREAD_LOCAL ThreadContext s_thread_context = {0};

///////////////////////////////////////
// Stopwatch
///////////////////////////////////////

Stopwatch stopwatch_create(void)
{
    Stopwatch stopwatch = {0};
    stopwatch.entry_count = 1; // entry[0] is unaccounted time

    return stopwatch;
}

void stopwatch_reset(Stopwatch *stopwatch)
{
    if(!stopwatch) return;
    MemoryZero(stopwatch, sizeof(Stopwatch));
    stopwatch->entry_count = 1;
}

static int stopwatch_get_entry_index(Stopwatch *stopwatch, String label)
{
    u32 hash = hash_string(label, 0);

    s64 index = -1;
    for(s64 i = 1; i < MIN(stopwatch->entry_count, STOPWATCH_MAX_ENTRIES); ++i)
    {
        StopwatchEntry *entry = &stopwatch->entries[i];

        if(entry->hash == hash)
        {
            index = i;
            break;
        }
    }

    if(index == -1)
    {
        if(stopwatch->entry_count >= STOPWATCH_MAX_ENTRIES)
        {
            logw("Hit max Stopwatch entries, allocating to entry 0");
            index = 0;
        }
        else
        {
            index = stopwatch->entry_count++;
            stopwatch->entries[index].hash = hash;
            stopwatch->entries[index].label = label;
        }
    }

    return index;
}

void stopwatch_begin(Stopwatch *stopwatch, String label)
{
    if(!stopwatch) return;

    int index = stopwatch_get_entry_index(stopwatch, label);
    stopwatch->entries[index].start_value = os_time_value_u64();
}

void stopwatch_end(Stopwatch *stopwatch, String label)
{
    if(!stopwatch) return;

    int index = stopwatch_get_entry_index(stopwatch, label);

    StopwatchEntry *entry = &stopwatch->entries[index];

    if(entry->start_value > 0)
    {
        f64 elapsed_time = (os_time_value_u64() - entry->start_value) / (f64)_timer.frequency;
        entry->total_seconds += elapsed_time;
    }
}

void stopwatch_print(Stopwatch *stopwatch)
{
    if(!stopwatch) return;

    f64 total = 0.0;
    for(s64 i = 0; i < stopwatch->entry_count; ++i)
    {
        total += stopwatch->entries[i].total_seconds;
    }

    const char *dots = "....................";

    logi("============== STOPWATCH ==============");
    for(s64 i = 1; i < stopwatch->entry_count; ++i)
    {
        StopwatchEntry *entry = &stopwatch->entries[i];
        String label = i == 0 ? S("(none)") : entry->label;
        s32 num_dots = MAX(0, 20 - label.len);
        logi("  " STR_FMT "%.*s%8.6f s (%05.2f%%)", STR_ARG(label), num_dots, dots, entry->total_seconds, 100.0*(entry->total_seconds / total));
    }

    logi("---------------------------------------");
    logi("  %s...............%8.6f s", "TOTAL", total);
    logi("=======================================");
}

///////////////////////////////////////
// File Helpers
///////////////////////////////////////

OS_File os_file_nil(void)
{
    OS_File file = {0};
    return file;
}

inline LogLevel os_get_log_level(void)
{
    return s_log_level;
}

void os_set_log_level(LogLevel level)
{
    s_log_level = level;
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
    return os_file_open(file_path, OS_READABLE | OS_WRITABLE);
}

void os_file_reset_pos(OS_File file)
{
    os_file_set_pos(file, 0);
}

s64 os_file_get_length_to_char(OS_File file, char c)
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

String os_file_read_line(Arena *arena, OS_File file)
{
    String str = {0};

    s64 len = os_file_get_length_to_char(file, '\n');
    if(len == 0) return str;

    str.data = PUSH_ARRAY(arena, u8, len);

    for(s64 i = 0; i < len; ++i)
    {
        s32 c = os_file_read_char(file);
        str.data[str.len++] = (u8)c;
    }

    os_file_read_char(file); // read one more to discard newline

    return str;
}

StringArray os_file_read_lines(Arena *arena, OS_File file)
{
    String str = os_file_read_to_string(arena, file);
    StringArray sa = string_split(arena, str, S("\n"));

    return sa;
}

u64 os_file_get_remaining_size(OS_File file)
{
    s64 size = os_file_get_size(file);
    s64 bytes_left = (size - os_file_get_pos(file));
    bytes_left = MAX(0, bytes_left);
    return (u64)bytes_left;
}

u8 os_file_read_u8(Arena *arena, OS_File file)
{
    u8 ret = 0;

    ByteArray ba = os_file_read(arena, file, sizeof(u8));
    if(ba.len >= sizeof(u8))
        ret = *((u8 *)ba.data);

    return ret;
}

u16 os_file_read_u16(Arena *arena, OS_File file)
{
    u16 ret = 0;

    ByteArray ba = os_file_read(arena, file, sizeof(u16));
    if(ba.len >= sizeof(u16))
        ret = *((u16 *)ba.data);

    return ret;
}

u32 os_file_read_u32(Arena *arena, OS_File file)
{
    u32 ret = 0;

    ByteArray ba = os_file_read(arena, file, sizeof(u32));
    if(ba.len >= sizeof(u32))
        ret = *((u32 *)ba.data);

    return ret;
}

u64 os_file_read_u64(Arena *arena, OS_File file)
{
    u64 ret = 0;

    ByteArray ba = os_file_read(arena, file, sizeof(u64));
    if(ba.len >= sizeof(u64))
        ret = *((u64 *)ba.data);

    return ret;
}

f32 os_file_read_f32(Arena *arena, OS_File file)
{
    f32 ret = 0;

    ByteArray ba = os_file_read(arena, file, sizeof(f32));
    if(ba.len >= sizeof(f32))
        ret = *((f32 *)ba.data);

    return ret;
}

f64 os_file_read_f64(Arena *arena, OS_File file)
{
    f64 ret = 0;

    ByteArray ba = os_file_read(arena, file, sizeof(f64));
    if(ba.len >= sizeof(f64))
        ret = *((f64 *)ba.data);

    return ret;
}

String os_file_read_str(Arena *arena, OS_File file)
{
    String str = {0};

    u64 str_len = os_file_read_u64(arena, file);
    ByteArray ba = os_file_read(arena, file, str_len);

    str.len = str_len;
    str.data = ba.data;

    return str;
}

inline s32 os_file_write_u8(OS_File file, u8 x)   { return os_file_write(file, &x, sizeof(u8)); }
inline s32 os_file_write_u16(OS_File file, u16 x) { return os_file_write(file, &x, sizeof(u16)); }
inline s32 os_file_write_u32(OS_File file, u32 x) { return os_file_write(file, &x, sizeof(u32)); }
inline s32 os_file_write_u64(OS_File file, u64 x) { return os_file_write(file, &x, sizeof(u64)); }
inline s32 os_file_write_f32(OS_File file, f32 x) { return os_file_write(file, &x, sizeof(f32)); }
inline s32 os_file_write_f64(OS_File file, f64 x) { return os_file_write(file, &x, sizeof(f64)); }
inline s32 os_file_write_str(OS_File file, String str) 
{
    s32 size_count = os_file_write(file, &str.len, sizeof(u64));
    s32 data_count = os_file_write(file, str.data, str.len);
    return size_count + data_count;
}
inline s32 os_file_write_u32_at_index(OS_File file, u32 x, u32 index)
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

    for(s64 i = 0; i < files.count; ++i)
    {
        String file = files.items[i];
        String file_lowercase = string_to_lower(arena, file);

        for(s64 j = 0; j < extensions.count; ++j)
        {
            String ext = extensions.items[j];
            if(ext.len == 0) continue;

            if(string_ends_with(file_lowercase, ext))
            {
                // add to new list
                string_list_add(&filtered, file_lowercase);
            }
        }
    }

    return string_list_to_array(filtered);
}


///////////////////////////////////////
// File Paths
///////////////////////////////////////

String os_path_get_extension(String path)
{
    s64 i = 0;
    b32 found = false;

    for(i = path.len - 1; i >= 0; --i)
    {
        u8 c = path.data[i];
        if(c == '.')
        {
            i++;
            found = true;
            break;
        }
    }

    String extension = {0};
    if(!found) return extension;

    extension.len = path.len - i;
    extension.data = &path.data[i];

    return extension;
}

String os_path_get_directory(String path)
{
    s64 i = 0;
    b32 found = false;

    for(i = path.len - 1; i >= 0; --i)
    {
        u8 c = path.data[i];
        if(c == OS_PATH_SLASH)
        {
            found = true;
            break;
        }
    }

    String directory = {0};
    if(!found) return directory;

    directory.len = i;
    directory.data = &path.data[0];

    return directory;

}

String os_path_get_file(String path)
{
    s64 i = 0;
    b32 found = false;

    for(i = path.len - 1; i >= 0; --i)
    {
        u8 c = path.data[i];
        if(c == OS_PATH_SLASH)
        {
            i++;
            found = true;
            break;
        }
    }

    String file = {0};
    if(!found) return file;

    file.len = path.len - i;
    file.data = &path.data[i];

    return file;
}

///////////////////////////////////////
// Threads
///////////////////////////////////////

ThreadValuesRange thread_range(u64 values_count)
{
    s64 thread_index = s_thread_context.index;
    s64 thread_count = s_thread_context.count;

    ThreadValuesRange range = {0};

    s64 values_per_thread = values_count / thread_count;
    s64 leftover_values_count = values_count % thread_count;
    b32 thread_has_leftover = (thread_index < leftover_values_count);
    s64 leftovers_before_this_thread_index = 
        (thread_has_leftover ? thread_index : leftover_values_count);
    s64 thread_first_value_idx = (values_per_thread * thread_index +
                                  leftovers_before_this_thread_index);
    s64 thread_opl_value_idx = (thread_first_value_idx + values_per_thread + 
                                !!thread_has_leftover);

    range.min = thread_first_value_idx;
    range.max = thread_opl_value_idx;

    return range;
}


