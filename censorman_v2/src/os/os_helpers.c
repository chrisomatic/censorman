
// The following functions are OS-agnostic,
// derived from the core OS functions

LogLevel _system_log_level = LOG_LEVEL_DEBUG;

///////////////////////////////////////
// File Helpers
///////////////////////////////////////

OS_File os_file_nil()
{
    OS_File file = {0};
    return file;
}

inline LogLevel os_get_log_level(void)
{
    return _system_log_level;
}

void os_set_log_level(LogLevel level)
{
    _system_log_level = level;
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

inline s32 os_file_write_u8(OS_File file, u8 x)   { return os_file_write(file, &x, sizeof(u8)); }
inline s32 os_file_write_u16(OS_File file, u16 x) { return os_file_write(file, &x, sizeof(u16)); }
inline s32 os_file_write_u32(OS_File file, u32 x) { return os_file_write(file, &x, sizeof(u32)); }
inline s32 os_file_write_f32(OS_File file, f32 x) { return os_file_write(file, &x, sizeof(f32)); }
inline s32 os_file_write_str(OS_File file, String str) { return os_file_write(file, str.data, str.len); }
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

    for(int i = 0; i < files.count; ++i)
    {
        String file = files.items[i];
        String file_lowercase = string_to_lower(arena, file);

        for(int j = 0; j < extensions.count; ++j)
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
    int i = 0;
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
    int i = 0;
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
    int i = 0;
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
