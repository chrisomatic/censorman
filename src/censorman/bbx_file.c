
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
    printf("\n");
    printf("BBX FILE FORMAT (V%d)\n", BBX_VERSION);
    printf("\n");
    printf("A binary format that contains bounding box data for a list\n");
    printf("of video and image assets.\n");
    printf("\n");
    printf("|---------------------------------------------------------------|\n");
    printf("|      name      |    format   | byte length |   description    |\n");
    printf("|----------------|-------------|-------------|------------------|<......    \n");
    printf("| Magic Word     | char array  | 3           | 'BBX'            |      :    \n");
    printf("| Version        | u8          | 1           |  2               |   preamble\n");
    printf("| Asset Count    | u32         | 4           | Number of Assets |      :    \n");
    printf("|----------------|-------------|-------------|------------------|<......    \n");
    printf("| Index          | u32         | 4           | Asset Index      |      :    \n");
    printf("| Type           | u8          | 1           | 1:Image 2:Video  |      :    \n");
    printf("| Path Len       | u64         | 8           | File Path Length |      :    \n");
    printf("| Path           | char array  | <path len>  | Of Asset File    |   asset N \n");
    printf("| Width          | u16         | 2           | In Pixels        |      :    \n");
    printf("| Height         | u16         | 2           | In Pixels        |      :    \n");
    printf("| FPS            | float32     | 4           | Frames Per Sec   |      :    \n");
    printf("| Frame Count    | u32         | 4           | Number of Frames |      :    \n");
    printf("|----------------|-------------|-------------|------------------|<......    \n");
    printf("| Frame Number   | u32         | 4           | Frame Index      |      :    \n");
    printf("| Box Count      | u32         | 4           | Number of Boxes  |   frame M \n");
    printf("| Interpolated   | u8          | 1           | 0:False 1:True   |      :    \n");
    printf("|----------------|-------------|-------------|------------------|<......    \n");
    printf("| Box Type       | u8          | 1           | <detect type>    |      :    \n");
    printf("| Position X     | u16         | 2           | In Image         |      :    \n");
    printf("| Position Y     | u16         | 2           | In Image         |      :    \n");
    printf("| Width          | u16         | 2           | In Pixels        |      :    \n");
    printf("| Height         | u16         | 2           | In Pixels        |      :    \n");
    printf("| Confidence     | u8          | 1           | [0-100]          |      :    \n");
    printf("| Landmark 1 X   | u16         | 2           | Left Eye X       |      :    \n");
    printf("| Landmark 1 Y   | u16         | 2           | Left Eye Y       |    box B  \n");
    printf("| Landmark 2 X   | u16         | 2           | Right Eye X      |      :    \n");
    printf("| Landmark 2 Y   | u16         | 2           | Right Eye Y      |      :    \n");
    printf("| Landmark 3 X   | u16         | 2           | Nose X           |      :    \n");
    printf("| Landmark 3 Y   | u16         | 2           | Nose Y           |      :    \n");
    printf("| Landmark 4 X   | u16         | 2           | Mouth Left X     |      :    \n");
    printf("| Landmark 4 Y   | u16         | 2           | Mouth Left Y     |      :    \n");
    printf("| Landmark 5 X   | u16         | 2           | Mouth Right X    |      :    \n");
    printf("| Landmark 5 Y   | u16         | 2           | Mouth Right Y    |      :    \n");
    printf("|----------------|-------------|-------------|------------------|<......    \n");
    printf("\n");
    printf("N := Range [0...Asset Count]\n");
    printf("M := Range [0...Frame Count]\n");
    printf("B := Range [0.....Box Count]\n");
    printf("\n");
    printf("MORE TABLES\n");
    printf("\n");
    printf("|---------------|-------|\n");
    printf("| detect type   | value |\n");
    printf("|---------------|-------|\n");
    printf("| face          | 1     |\n");
    printf("| person        | 2     |\n");
    printf("| license_plate | 3     |\n");
    printf("| nudity        | 4     |\n");
    printf("|...............|.......|\n");
    printf("| eye           | 16    |\n");
    printf("| nose          | 17    |\n");
    printf("| mouth         | 18    |\n");
    printf("|---------------|-------|\n");
}
