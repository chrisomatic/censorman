#pragma once

#include "base.h"

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

    String* files = (String*)arena_alloc(arena, sizeof(String) * 1024); // adjust size as needed

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            String name = str_from_cstr(find_data.cFileName);
            bool has_valid_extension = false;
            for(int i = 0; i < extension_count; ++i)
            {
                has_valid_extension |= str_ends_with(name, extensions[i]);
            }
            if (has_valid_extension) {
                char* file_copy = (char*)arena_alloc(arena, name.len + 1);
                memcpy(file_copy, name.data, name.len);
                file_copy[name.len] = '\0';

                files[file_count++] = (String){ .len = name.len, .data = file_copy };
            }
        }
    } while (FindNextFileA(handle, &find_data));

    FindClose(handle);

    *out_files = files;

#elif PLATFORM == PLATFORM_UNIX || PLATFORM == PLATFORM_MAC

    char path_buffer[1024];
    snprintf(path_buffer, sizeof(path_buffer), "%.*s", folder_path.len, folder_path.data);

    DIR* dir = opendir(path_buffer);
    if (!dir) return 0;

    struct dirent* entry;
    String* files = (String*)arena_alloc(arena, sizeof(String) * 1024); // adjust size as needed

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_DIR) {
            String name = str_from_cstr(entry->d_name);

            bool has_valid_extension = false;
            for(int i = 0; i < extension_count; ++i)
            {
                has_valid_extension |= str_ends_with(name, extensions[i]);
            }
            if (has_valid_extension) {
                char* file_copy = (char*)arena_alloc(arena, name.len + 1);
                memcpy(file_copy, name.data, name.len);
                file_copy[name.len] = '\0';

                files[file_count++] = (String){ .len = name.len, .data = file_copy };
            }
        }
    }
    closedir(dir);
    *out_files = files;

#else
    #error "Unsupported platform"
#endif

    return file_count;
}
