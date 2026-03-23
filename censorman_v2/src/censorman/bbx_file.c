
OS_File bbx_file_create(String file_path)
{
    Temp scratch = scratch_begin();
    char *file_path_cstr = string_to_cstr(scratch.arena, file_path);
    OS_File file = os_file_create_and_open(file_path_cstr, OS_WRITABLE);
    scratch_end(scratch);

    return file;
}

void bbx_file_write_header(OS_File file, s32 w, s32 h, u32 total_frame_count)
{
    os_file_write_str(file, S("BBX"));
    os_file_write_u8(file,  BBX_VERSION);
    os_file_write_u16(file, (u16)w);
    os_file_write_u16(file, (u16)h);
    os_file_write_f32(file, 0.0);
    os_file_write_u32(file, total_frame_count);
}

void bbx_file_write_total_frame_count(OS_File file, u32 total_frame_count)
{
    os_file_write_u32_at_index(file, total_frame_count, BBX_FRAME_COUNT_OFFSET);
}

void bbx_file_write_box_frame(OS_File file, BoxFrame *frame)
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
        os_file_write_u16(file, box->confidence);

        for(s32 i = 0; i < LANDMARK_COUNT; ++i)
        {
            os_file_write_u16(file, box->landmarks[i].x);
            os_file_write_u16(file, box->landmarks[i].y);
        }
    }
}

void bbx_file_close(OS_File file)
{
    os_file_close(file);
}
