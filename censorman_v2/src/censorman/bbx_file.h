#pragma once

#define BBX_VERSION 2
#define BBX_FRAME_COUNT_OFFSET 12

OS_File bbx_file_create(String file_path);

void bbx_file_write_header(OS_File file, s32 w, s32 h, u32 total_frame_count);
void bbx_file_write_total_frame_count(OS_File file, u32 total_frame_count);
void bbx_file_write_box_frame(OS_File file, BoxFrame *frame);
void bbx_file_close(OS_File file);
