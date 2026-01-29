#pragma once

#include "base.h"

#if 0 //PLATFORM == PLATFORM_WINDOWS

HANDLE *threads  = NULL;

bool thread_init(int count)
{
    threads = (HANDLE *)calloc(count,sizeof(HANDLE));
    return false;
}

int thread_create(HANDLE *thread, void *(func)(void *), void *arg)
{
    *thread = CreateThread(NULL, 0, func, arg, 0, NULL);
    return (*thread != NULL);
}

int thread_join(HANDLE thread)
{
    return WaitForSingleObject(thread, INFINITE);
}

#else

pthread_t *threads = NULL;

bool thread_init(int count)
{
    threads = (pthread_t *)calloc(count,sizeof(pthread_t));
    return (threads != NULL);
}

int thread_create(pthread_t *thread, void *(func)(void *), void *arg)
{
    return (pthread_create(thread, NULL, func, arg));
}

int thread_join(pthread_t thread)
{
    return (pthread_join(thread,NULL));
}

#endif


int platform_get_files_in_folder(Arena* arena, String folder_path, String* extensions, int extension_count, String** out_files)
{
    int file_count = 0;

#if PLATFORM == PLATFORM_WINDOWS

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
            bool has_valid_extension = false;
            for(int i = 0; i < extension_count; ++i)
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


            bool has_valid_extension = false;
            for(int i = 0; i < extension_count; ++i)
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
