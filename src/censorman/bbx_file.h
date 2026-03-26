#pragma once

#define BBX_VERSION 2
#define BBX_FRAME_COUNT_OFFSET 12

OS_File bbx_file_create(String file_path);

void bbx_file_write_preamble(OS_File file, u32 asset_count);
void bbx_file_write_asset_header(OS_File file, u32 asset_index, Asset *asset, s32 w, s32 h, f32 fps, u32 total_frame_count);
void bbx_file_write_box_frame(OS_File file, BoxFrame *frame);
void bbx_file_close(OS_File file);

void bbx_file_parse_and_print(String bbx_file_path);
void bbx_print_format(void);
