
OS_File bbx_file_create(String file_path)
{
    OS_File file = {0};

    Temp scratch = scratch_begin();
    char *file_path_cstr = string_to_cstr(scratch.arena, file_path);
    file = os_file_create_and_open(file_path_cstr, OS_WRITABLE);
    scratch_end(scratch);

    return file;
}

void bbx_file_write_preamble(OS_File file, u32 asset_count)
{
    if(file.is_valid)
    {
        os_file_write_u8(file, 'B');
        os_file_write_u8(file, 'B');
        os_file_write_u8(file, 'X');
        os_file_write_u8(file,  BBX_VERSION);
        os_file_write_u32(file, asset_count);
    }
}

void bbx_file_write_asset_header(OS_File file, u32 asset_index, Asset *asset, s32 w, s32 h, f32 fps, u32 total_frame_count)
{
    if(file.is_valid)
    {
        os_file_write_u32(file, asset_index);
        os_file_write_u8(file, asset->type);
        os_file_write_str(file, asset->path);
        os_file_write_u16(file, (u16)w);
        os_file_write_u16(file, (u16)h);
        os_file_write_f32(file, fps);
        os_file_write_u32(file, total_frame_count);
    }
}

void bbx_file_write_box_frame(OS_File file, BoxFrame *frame)
{
    if(file.is_valid)
    {
        os_file_write_u32(file, frame->frame_number);
        os_file_write_u32(file, frame->box_count);
        os_file_write_u8(file,  frame->interpolated ? 0x01 : 0x00);

        for(s32 i = 0; i < frame->box_count; ++i)
        {
            Box *box = &frame->boxes[i];

            os_file_write_u8(file, box->type);

            os_file_write_u16(file, box->x);
            os_file_write_u16(file, box->y);
            os_file_write_u16(file, box->w);
            os_file_write_u16(file, box->h);
            os_file_write_u8(file, box->confidence);

            for(s32 i = 0; i < LANDMARK_COUNT; ++i)
            {
                os_file_write_u16(file, box->landmarks[i].x);
                os_file_write_u16(file, box->landmarks[i].y);
            }
        }
    }
}

void bbx_file_close(OS_File file)
{
    if(file.is_valid)
    {
        os_file_close(file);
    }
}

void bbx_file_parse_and_print(String bbx_file_path)
{
    Temp scratch = scratch_begin();

    char *cstr = string_to_cstr(scratch.arena, bbx_file_path);
    OS_File file = os_file_open_readonly(cstr);

    if(file.is_valid)
    {

        // parse out bbx file
        logi("BBX File '%s':", cstr);

        // preamble
        ByteArray bbx_magic = os_file_read(scratch.arena, file, 3);
        u8 version          = os_file_read_u8(scratch.arena, file);
        u32 asset_count     = os_file_read_u32(scratch.arena, file);

        logi("  bbx_magic:   %.*s", bbx_magic.len, bbx_magic.data);
        logi("  version:     %u", version);
        logi("  asset count: %u", asset_count);

        for(s64 i = 0; i < asset_count; ++i)
        {
            u32    asset_index = os_file_read_u32(scratch.arena, file);
            u8     asset_type  = os_file_read_u8(scratch.arena, file);
            String asset_path  = os_file_read_str(scratch.arena, file);
            u16    w           = os_file_read_u16(scratch.arena, file);
            u16    h           = os_file_read_u16(scratch.arena, file);
            f32    fps         = os_file_read_f32(scratch.arena, file);
            u32    frame_count = os_file_read_u32(scratch.arena, file);

            logi("  [%u] (" STR_FMT "): " STR_FMT, asset_index, STR_ARG(asset_type_to_string(asset_type)), STR_ARG(asset_path));
            logi("    w: %u, h: %u", w, h);
            logi("    fps: %f", fps);
            logi("    frame count: %u", frame_count);

            for(s64 j = 0; j < frame_count; ++j)
            {
                u32 frame_number = os_file_read_u32(scratch.arena, file);
                u32 box_count = os_file_read_u32(scratch.arena, file);
                u8 interpolated = os_file_read_u8(scratch.arena, file);

                logi("    frame %u (box count: %u) interpolated: %s", frame_number, box_count, interpolated == 0x00 ? "No" : "Yes");

                for(s64 k = 0; k < box_count; ++k)
                {
                    Box box = {0};

                    box.type = os_file_read_u8(scratch.arena, file);

                    box.x = os_file_read_u16(scratch.arena, file);
                    box.y = os_file_read_u16(scratch.arena, file);
                    box.w = os_file_read_u16(scratch.arena, file);
                    box.h = os_file_read_u16(scratch.arena, file);
                    box.confidence = os_file_read_u8(scratch.arena, file);

                    for(s32 l = 0; l < LANDMARK_COUNT; ++l)
                    {
                        box.landmarks[l].x = os_file_read_u16(scratch.arena, file);
                        box.landmarks[l].y = os_file_read_u16(scratch.arena, file);
                    }

                    box_print(&box, LOG_LEVEL_INFO);
                }
            }
        }
    }

    scratch_end(scratch);
}

void bbx_print_format(void)
{
    os_printf("\n");
    os_printf("BBX FILE FORMAT (V%d)\n", BBX_VERSION);
    os_printf("\n");
    os_printf("A binary format that contains bounding box data for a list\n");
    os_printf("of video and image assets.\n");
    os_printf("\n");
    os_printf("|---------------------------------------------------------------|\n");
    os_printf("|      name      |    format   | byte length |   description    |\n");
    os_printf("|----------------|-------------|-------------|------------------|<......    \n");
    os_printf("| Magic Word     | char array  | 3           | 'BBX'            |      :    \n");
    os_printf("| Version        | u8          | 1           |  2               |   preamble\n");
    os_printf("| Asset Count    | u32         | 4           | Number of Assets |      :    \n");
    os_printf("|----------------|-------------|-------------|------------------|<.....:    \n");
    os_printf("| Index          | u32         | 4           | Asset Index      |      :    \n");
    os_printf("| Type           | u8          | 1           | 1:Image 2:Video  |      :    \n");
    os_printf("| Path Len       | u64         | 8           | File Path Length |      :    \n");
    os_printf("| Path           | char array  | <path len>  | Of Asset File    |   asset N \n");
    os_printf("| Width          | u16         | 2           | In Pixels        |      :    \n");
    os_printf("| Height         | u16         | 2           | In Pixels        |      :    \n");
    os_printf("| FPS            | float32     | 4           | Frames Per Sec   |      :    \n");
    os_printf("| Frame Count    | u32         | 4           | Number of Frames |      :    \n");
    os_printf("|----------------|-------------|-------------|------------------|<.....:    \n");
    os_printf("| Frame Number   | u32         | 4           | Frame Index      |      :    \n");
    os_printf("| Box Count      | u32         | 4           | Number of Boxes  |   frame M \n");
    os_printf("| Interpolated   | u8          | 1           | 0:False 1:True   |      :    \n");
    os_printf("|----------------|-------------|-------------|------------------|<.....:    \n");
    os_printf("| Box Type       | u8          | 1           | <detect type>    |      :    \n");
    os_printf("| Position X     | u16         | 2           | In Image         |      :    \n");
    os_printf("| Position Y     | u16         | 2           | In Image         |      :    \n");
    os_printf("| Width          | u16         | 2           | In Pixels        |      :    \n");
    os_printf("| Height         | u16         | 2           | In Pixels        |      :    \n");
    os_printf("| Confidence     | u8          | 1           | [0-100]          |      :    \n");
    os_printf("| Landmark 1 X   | u16         | 2           | Left Eye X       |      :    \n");
    os_printf("| Landmark 1 Y   | u16         | 2           | Left Eye Y       |    box B  \n");
    os_printf("| Landmark 2 X   | u16         | 2           | Right Eye X      |      :    \n");
    os_printf("| Landmark 2 Y   | u16         | 2           | Right Eye Y      |      :    \n");
    os_printf("| Landmark 3 X   | u16         | 2           | Nose X           |      :    \n");
    os_printf("| Landmark 3 Y   | u16         | 2           | Nose Y           |      :    \n");
    os_printf("| Landmark 4 X   | u16         | 2           | Mouth Left X     |      :    \n");
    os_printf("| Landmark 4 Y   | u16         | 2           | Mouth Left Y     |      :    \n");
    os_printf("| Landmark 5 X   | u16         | 2           | Mouth Right X    |      :    \n");
    os_printf("| Landmark 5 Y   | u16         | 2           | Mouth Right Y    |      :    \n");
    os_printf("|----------------|-------------|-------------|------------------|<......    \n");
    os_printf("\n");
    os_printf("N := Range [0...Asset Count]\n");
    os_printf("M := Range [0...Frame Count]\n");
    os_printf("B := Range [0.....Box Count]\n");
    os_printf("\n");
    os_printf("MORE TABLES\n");
    os_printf("\n");
    os_printf("|---------------|-------|\n");
    os_printf("| detect type   | value |\n");
    os_printf("|---------------|-------|\n");
    os_printf("| face          | 1     |\n");
    os_printf("| person        | 2     |\n");
    os_printf("| license_plate | 3     |\n");
    os_printf("| nudity        | 4     |\n");
    os_printf("|...............|.......|\n");
    os_printf("| eye           | 16    |\n");
    os_printf("| nose          | 17    |\n");
    os_printf("| mouth         | 18    |\n");
    os_printf("|---------------|-------|\n");
}
