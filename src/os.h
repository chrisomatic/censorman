#pragma once

#include "base.h"

pthread_t *threads = NULL;

b32 thread_init(s32 count)
{
    threads = (pthread_t *)calloc(count,sizeof(pthread_t));
    return (threads != NULL);
}

s32 thread_create(pthread_t *thread, void *(func)(void *), void *arg)
{
    return (pthread_create(thread, NULL, func, arg));
}

s32 thread_join(pthread_t thread)
{
    return (pthread_join(thread,NULL));
}

///////////////////////////////////////
// Files
///////////////////////////////////////

typedef enum
{
    OS_READABLE = (1 << 0),
    OS_WRITABLE = (1 << 1),
} OS_FileProps;

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

typedef struct OS_File OS_File;

b32         os_file_exists(char *file_path);
OS_File     os_file_open(char *file_path, u32 props);
OS_File     os_file_create_and_open(char *file_path, u32 props);
b32         os_file_create(char *file_path);
b32         os_file_create_directory(String dir_path);
void        os_file_close(OS_File file);

b32         os_file_is_valid(OS_File file);
s64         os_file_get_size(OS_File file);
s64         os_file_get_length_to_char(OS_File file, char c);

String      os_file_read_to_string(Arena *arena, OS_File file);
String      os_file_read_line(Arena *arena, OS_File file);
StringArray os_file_read_lines(Arena *arena, OS_File file);

s64 os_file_get_pos(OS_File file);
void os_file_set_pos(OS_File file, s64 pos);
void os_file_reset_pos(OS_File file);

OS_File os_file_nil();
OS_File os_file_open_readonly(char *file_path);
OS_File os_file_open_writeonly(char *file_path);
OS_File os_file_open_readwrite(char *file_path);
OS_File os_file_create_and_open(char *file_path, u32 props);

inline s32 os_file_write(OS_File file, void *data, s32 size);
inline s32 os_file_write_u8(OS_File file, u8 x);
inline s32 os_file_write_u16(OS_File file, u16 x);
inline s32 os_file_write_u32(OS_File file, u32 x);
inline s32 os_file_write_f32(OS_File file, f32 x);
inline s32 os_file_write_str(OS_File file, String str);
inline s32 os_file_write_u32_at_index(OS_File file, u32 x, u32 index);

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
// Files
///////////////////////////////////////

#if OS == OS_WINDOWS

#define OS_PATH_SLASH '\\'

static u32 _map_props_to_access(u32 props)
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

OS_File os_file_open(char *file_path, u32 props)
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

OS_File os_file_create_and_open(char *file_path, u32 props)
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
    handle = CreateFile(file_path, NULL, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    b32 is_valid = (handle != INVALID_HANDLE_VALUE);

    if(is_valid) CloseHandle(handle);
    return is_valid;
}

b32 os_file_create_directory(String dir_path)
{
    Temp scratch = scratch_begin();
    char *cstr = string_to_cstr(scratch.arena, dir_path);
    s32 ret = CreateDirectory(cstr, NULL);
    scratch_end(scratch);
    return (ret != 0);
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

#else

#define OS_PATH_SLASH '/'

static s32 _map_props_to_access(u32 props)
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

OS_File os_file_open(char *file_path, u32 props)
{
    s32 access = _map_props_to_access(props);

    if(access == -1)
        return os_file_nil();

    s32 fd = open(file_path, access);

    OS_File file = {
        .handle = fd,
        .props = (OS_FileProps)props
    };

    file.is_valid = (fd > -1);
    return file;
}

OS_File os_file_create_and_open(char *file_path, u32 props)
{
    s32 access = _map_props_to_access(props);
    if(access == -1) access = O_CREAT | O_TRUNC;

    s32 fd = open(file_path, access, 0644);

    OS_File file = {
        .handle = fd,
        .props = (OS_FileProps)props
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

b32 os_file_create_directory(String dir_path)
{
    Temp scratch = scratch_begin();
    char *cstr = string_to_cstr(scratch.arena, dir_path);
    s32 err = mkdir(cstr, 0755);
    scratch_end(scratch);
    return (err == 0);
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

s32 os_file_write(OS_File file, void *data, s32 size)
{
    return write(file.handle, data, size);
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

    str.data = (u8 *)PUSH_ARRAY(arena, u8, len);

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
                string_list_add(&filtered, file);
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


s32 platform_get_files_in_folder(Arena* arena, String folder_path, String* extensions, s32 extension_count, String** out_files)
{
    s32 file_count = 0;

#if OS == OS_WINDOWS

    // Construct search pattern (e.g., "folder_path\*")
    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%.*s\\*", folder_path.len, folder_path.data);

    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(search_path, &find_data);

    if (handle == INVALID_HANDLE_VALUE) return 0;

    String* files = (String*)PUSH_ARRAY(arena, String, 1024);

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            String name = STR(find_data.cFileName);
            b32 has_valid_extension = false;
            for(s32 i = 0; i < extension_count; ++i)
            {
                has_valid_extension |= string_ends_with(name, extensions[i]);
            }
            if (has_valid_extension) {
                char* file_copy = (char*)PUSH_ARRAY(arena, char, name.len+1);
                memcpy(file_copy, name.data, name.len);
                file_copy[name.len] = '\0';

                files[file_count++] = (String){ .len = name.len, .data = file_copy };
            }
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);

    *out_files = files;

#else

    char path_buffer[1024];
    snprintf(path_buffer, sizeof(path_buffer), "%.*s", folder_path.len, folder_path.data);

    DIR* dir = opendir(path_buffer);
    if (!dir) return 0;

    struct dirent* entry;
    String* files = (String*)PUSH_ARRAY(arena, String, 1024);

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) {
            String name = STR(entry->d_name);


            b32 has_valid_extension = false;
            for(s32 i = 0; i < extension_count; ++i)
            {
                has_valid_extension |= string_ends_with(name, extensions[i]);
            }
            if (has_valid_extension) {
                char* file_copy = (char*)PUSH_ARRAY(arena, char, name.len + 1);
                memcpy(file_copy, name.data, name.len);
                file_copy[name.len] = '\0';
                files[file_count++] = (String){ .len = name.len, .data = (u8 *)file_copy };
            }
        }
    }
    closedir(dir);
    *out_files = files;

#endif

    return file_count;
}
