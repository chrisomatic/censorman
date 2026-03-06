
Video video_nil()
{
    Video vid = {0};
    return vid;
}

Video video_begin(Arena *arena, String path, String out_path, u64 max_buffer_size, b32 no_encode)
{
    Video vid = {0};
    vid.arena = arena;

    VideoContext *ctx = &vid.context;

    char *path_cstr = string_to_cstr(arena, path);

    //////////////////
    // decoding     
    
    s32 open_input = avformat_open_input(&ctx->fmt_ctx, path_cstr, NULL, NULL);
    if(open_input < 0)
    {
        loge("Failed to open input file '%s'",path_cstr);
        return vid;
    }

    s32 find_stream_info = avformat_find_stream_info(ctx->fmt_ctx, NULL);
    if(find_stream_info < 0)
    {
        loge("Could not find stream info");
        avformat_close_input(&ctx->fmt_ctx);
        return vid;
    }

    ctx->stream_index = -1;

    for(u32 i = 0; i < ctx->fmt_ctx->nb_streams; ++i)
    {
        if(ctx->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            ctx->stream_index = i;
            break;
        }
    }

    s32 found_video_stream = (ctx->stream_index != -1);
    if(!found_video_stream)
    {
        loge("No video stream found");
        avformat_close_input(&ctx->fmt_ctx);
        return vid;
    }

    AVStream *stream = ctx->fmt_ctx->streams[ctx->stream_index];

    AVRational fps = stream->avg_frame_rate.num ? stream->avg_frame_rate : stream->r_frame_rate;
    if(fps.num == 0 || fps.den == 0) 
    {
        // fallback
        fps.num = 24;
        fps.den = 1;
    }

    vid.fps = (f64)fps.num / fps.den;

    // figure out rotation...
    s32 _rotation = 0;
    {
        // Try side_data in codecpar
        for (s32 i = 0; i < stream->codecpar->nb_coded_side_data; i++)
        {
            const AVPacketSideData *sd = &stream->codecpar->coded_side_data[i];

            if (sd->type == AV_PKT_DATA_DISPLAYMATRIX)
            {
                if (sd->size >= sizeof(s32) * 9)
                {
                    f64 angle = av_display_rotation_get((const s32*)sd->data);

                    // Normalize (FFmpeg returns e.g. 0.000001, — round)
                    s32 ang = (s32)round(angle);
                    ang = ((ang % 360) + 360) % 360;
                    if (ang == 0 || ang == 90 || ang == 180 || ang == 270)
                    {
                        _rotation = ang;
                    }
                    else
                    {
                        // If weird angle, set to 0
                        _rotation = 0;
                    }
                }
            }
        }

        // Fallback: metadata “rotate” tag
        AVDictionaryEntry *tag = av_dict_get(stream->metadata, "rotate", NULL, 0);
        if(tag && tag->value)
        {
            s32 ang = atoi(tag->value);
            ang = ((ang % 360) + 360) % 360;
            if (ang == 0 || ang == 90 || ang == 180 || ang == 270)
                _rotation = ang;
        }
    }

    vid.rotation = _rotation;

    enum AVCodecID codec_id = stream->codecpar->codec_id;
    ctx->codec = avcodec_find_decoder(codec_id);
    if(!ctx->codec)
    {
        loge("Unsupported codec");
        avformat_close_input(&ctx->fmt_ctx);
        return vid;
    }

    ctx->codec_ctx = avcodec_alloc_context3(ctx->codec);
    if(!ctx->codec_ctx)
    {
        loge("Could not allocate codec context");
        avformat_close_input(&ctx->fmt_ctx);
        return vid;
    }

    avcodec_parameters_to_context(ctx->codec_ctx, ctx->fmt_ctx->streams[ctx->stream_index]->codecpar);

    // Enable internal multithreading
    ctx->codec_ctx->thread_count = 0; // 0 = auto
    ctx->codec_ctx->thread_type = FF_THREAD_FRAME;

    b32 open_codec = avcodec_open2(ctx->codec_ctx, ctx->codec, NULL);
    if(open_codec < 0)
    {
        loge("Could not open codec");
        avcodec_free_context(&ctx->codec_ctx);
        avformat_close_input(&ctx->fmt_ctx);
        return vid;
    }

    s32 width          = ctx->codec_ctx->width;
    s32 height         = ctx->codec_ctx->height;
    u64 rgb_stride     = (u64)width * 3;
    u64 frame_rgb_size = rgb_stride * height;
    s32 max_frames     = floor(max_buffer_size / (f64)frame_rgb_size);

    vid.w                 = width;
    vid.h                 = height;
    vid.data              = PUSH_ARRAY(arena, RGBColor, (u64)(width * height * max_frames));
    vid.pts_buffer        = PUSH_ARRAY(arena, s64, max_frames);
    vid.frame_count_max   = max_frames;
    vid.frame_count_total = (stream->nb_frames > 0 ? stream->nb_frames : -1);

    if(!vid.data || !vid.pts_buffer)
    {
        loge("Failed to allocate RGB buffer of size %lu", (u64)frame_rgb_size * max_frames);
        avcodec_free_context(&ctx->codec_ctx);
        avformat_close_input(&ctx->fmt_ctx);
        return vid;
    }

    ctx->frame     = av_frame_alloc();
    ctx->rgb_frame = av_frame_alloc();
    ctx->pkt       = av_packet_alloc();

    u8 *rgb_planes[4];
    s32 rgb_linesize[4];

    av_image_fill_arrays(rgb_planes, rgb_linesize, (const u8 *)vid.data, AV_PIX_FMT_RGB24, vid.w, vid.h, 1);

    ctx->sws_ctx = sws_getContext(vid.w, vid.h, ctx->codec_ctx->pix_fmt,
                                  vid.w, vid.h, AV_PIX_FMT_RGB24,
                                  SWS_BILINEAR, NULL, NULL, NULL);

    //////////////////
    // encoding     

    if(no_encode) return vid;

    // output format: MP4/H264
    char *out_path_cstr = string_to_cstr(arena, out_path);

    avformat_alloc_output_context2(&ctx->enc_fmt_ctx, NULL, "mp4", out_path_cstr);
    if(!ctx->enc_fmt_ctx)
    {
        loge("Could not deduce output format");
        return vid;
    }

    ctx->enc_codec = avcodec_find_encoder_by_name("libx264");
    if(!ctx->enc_codec) ctx->enc_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if(!ctx->enc_codec) ctx->enc_codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if(!ctx->enc_codec)
    {
        loge("Encoder not found");
        avformat_free_context(ctx->enc_fmt_ctx);
        return vid;
    }

    // Add new video stream
    ctx->enc_stream = avformat_new_stream(ctx->enc_fmt_ctx, NULL);
    if(!ctx->enc_stream)
    {
        loge("Could not create stream");
        avformat_free_context(ctx->enc_fmt_ctx);
        return vid;
    }

    ctx->enc_codec_ctx = avcodec_alloc_context3(ctx->enc_codec);
    if(!ctx->enc_codec_ctx)
    {
        loge("Could not allocate codec context");
        avformat_free_context(ctx->enc_fmt_ctx);
        return vid;
    }

    // Basic encoding settings

    ctx->enc_codec_ctx->codec_id     = ctx->enc_codec->id;
    ctx->enc_codec_ctx->codec_type   = AVMEDIA_TYPE_VIDEO;
    ctx->enc_codec_ctx->width        = vid.w;
    ctx->enc_codec_ctx->height       = vid.h;
    ctx->enc_codec_ctx->time_base    = av_inv_q(fps);
    ctx->enc_codec_ctx->framerate    = fps;
    ctx->enc_codec_ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
    ctx->enc_codec_ctx->gop_size     = 12;
    ctx->enc_codec_ctx->max_b_frames = 0;
    ctx->enc_codec_ctx->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;
    ctx->enc_codec_ctx->thread_count = 0; // auto

    ctx->enc_stream->time_base  = ctx->enc_codec_ctx->time_base;

    if (ctx->enc_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->enc_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_dict_set(&ctx->enc_opts, "preset", "superfast", 0); // [ultrafast, superfast, fast, medium, slow, placebo]
    av_dict_set(&ctx->enc_opts, "tune", "zerolatency", 0);

    s32 ret;

    // Open encoder
    s32 open_encoder = avcodec_open2(ctx->enc_codec_ctx, ctx->enc_codec, &ctx->enc_opts);
    if(open_encoder < 0)
    {
        loge("Could not open encoder");
        avcodec_free_context(&ctx->enc_codec_ctx);
        avformat_free_context(ctx->enc_fmt_ctx);
        return vid;
    }

    // Copy codec params to stream
    s32 copy_codec_params = avcodec_parameters_from_context(ctx->enc_stream->codecpar, ctx->enc_codec_ctx);
    if(copy_codec_params < 0)
    {
        loge("Could not copy codec parameters");
        avcodec_free_context(&ctx->enc_codec_ctx);
        avformat_free_context(ctx->enc_fmt_ctx);
        return vid;
    }
    
    // set rotation on encoder stream
    char rotate[16];
    snprintf(rotate, sizeof(rotate), "%d", vid.rotation);
    av_dict_set(&ctx->enc_stream->metadata, "rotate", rotate, 0);

    // Open output file
    if(!(ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        s32 open_output_file = avio_open(&ctx->enc_fmt_ctx->pb, out_path_cstr, AVIO_FLAG_WRITE);
        if(open_output_file < 0)
        {
            loge("Could not open output file '%s'", out_path_cstr);
            avcodec_free_context(&ctx->enc_codec_ctx);
            avformat_free_context(ctx->enc_fmt_ctx);
            return vid;
        }
    }

    // Write header
    s32 write_header = avformat_write_header(ctx->enc_fmt_ctx, NULL);
    if(write_header < 0)
    {
        loge("Error occurred writing header");
        avio_close(ctx->enc_fmt_ctx->pb);
        avcodec_free_context(&ctx->enc_codec_ctx);
        avformat_free_context(ctx->enc_fmt_ctx);
        return vid;
    }

    // Allocate frame + packet
    ctx->enc_frame_src = av_frame_alloc();
    ctx->enc_frame     = av_frame_alloc();
    ctx->enc_pkt       = av_packet_alloc();

    if(!ctx->enc_frame || !ctx->enc_frame_src || !ctx->enc_pkt)
    {
        loge("Could not allocate frame/packet");
        return vid;
    }

    ctx->enc_frame_src->format = AV_PIX_FMT_RGB24;
    ctx->enc_frame_src->width  = vid.w;
    ctx->enc_frame_src->height = vid.h;

    ctx->enc_frame->format = ctx->enc_codec_ctx->pix_fmt;
    ctx->enc_frame->width  = ctx->enc_codec_ctx->width;
    ctx->enc_frame->height = ctx->enc_codec_ctx->height;
    ctx->enc_frame->pict_type = AV_PICTURE_TYPE_I;

    // SWS converter: RGB24 -> YUV420P
    ctx->enc_sws_ctx = sws_getContext(vid.w, vid.h, AV_PIX_FMT_RGB24,
                                      vid.w, vid.h, ctx->enc_codec_ctx->pix_fmt,
                                      SWS_BILINEAR, NULL, NULL, NULL);

    if(!ctx->enc_sws_ctx)
    {
        loge("Could not init sws context");
        return vid;
    }

    return vid;
}

b32 video_load_frames(Video *vid)
{
    if(!vid->data)
        return false;

    VideoContext *ctx = &vid->context;

    s32 width  = ctx->codec_ctx->width;
    s32 height = ctx->codec_ctx->height;
    s32 rgb_stride = width * 3;
    s32 frame_rgb_size = rgb_stride * height;

    u32 frame_count = 0;
    s32 ret;
    b32 eof_reached = false; // track end of file
    b32 hit_max_buffer = false;

    AVFrame *frame = ctx->frame;
    AVPacket *pkt  = ctx->pkt;

    while(frame_count < vid->frame_count_max)
    {
        ret = av_read_frame(ctx->fmt_ctx, pkt);
        if(ret < 0) // EOF or error
        {
            eof_reached = true;
            break;
        }

        if(pkt->stream_index != ctx->stream_index)
        {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(ctx->codec_ctx, pkt);
        av_packet_unref(pkt);
        if(ret < 0) break;

        while(ret >= 0)
        {
            ret = avcodec_receive_frame(ctx->codec_ctx, frame);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            else if(ret < 0) return false;

            // Convert to RGB
            u8 *dp = (u8 *)vid->data + (u64)frame_count * frame_rgb_size;
            u8 *dest_data[4] = { dp, NULL, NULL, NULL };
            s32 dest_linesize[4] = { rgb_stride, 0, 0, 0 };
            sws_scale(ctx->sws_ctx, (const u8 *const*)frame->data, frame->linesize,
                      0, height, dest_data, dest_linesize);

            vid->pts_buffer[frame_count] = frame->pts;
            frame_count++;
            if(frame_count >= vid->frame_count_max) {
                hit_max_buffer = true;
                break;
            }
        }
    }

    // flush decoder if EOF
    if(eof_reached)
    {
        avcodec_send_packet(ctx->codec_ctx, NULL);
        while(avcodec_receive_frame(ctx->codec_ctx, frame) == 0 && frame_count < vid->frame_count_max)
        {
            u8 *dp = (u8 *)vid->data + (u64)frame_count * frame_rgb_size;
            u8 *dest_data[4] = { dp, NULL, NULL, NULL };
            s32 dest_linesize[4] = { rgb_stride, 0, 0, 0 };
            sws_scale(ctx->sws_ctx, (const u8 *const*)frame->data, frame->linesize,
                      0, height, dest_data, dest_linesize);

            vid->pts_buffer[frame_count] = frame->pts;
            frame_count++;
        }
    }

    vid->frame_count = frame_count;
    vid->load_complete = !hit_max_buffer || eof_reached;

    return (frame_count > 0);

}

b32 video_save_frames(Video *vid)
{
    if(!vid->data || vid->frame_count == 0)
        return false;

    VideoContext *ctx = &vid->context;

    s32 width = vid->w;
    s32 height = vid->h;
    s32 rgb_stride = width * 3;
    s32 frame_rgb_size = rgb_stride * height;
    s32 ret;

    for(s32 i = 0; i < vid->frame_count; ++i)
    {
        av_frame_make_writable(ctx->enc_frame);

        // Source RGB data
        ctx->enc_frame_src->data[0] = (u8 *)vid->data + ((u64)i * frame_rgb_size);
        ctx->enc_frame_src->linesize[0] = rgb_stride;

        // Convert RGB -> YUV420P
        ret = sws_scale_frame(ctx->enc_sws_ctx, ctx->enc_frame, ctx->enc_frame_src);
        if(ret < 0)
        {
            logw("Error scaling frame %d", i);
            continue;
        }

        // Assign continuous PTS based on chunk index + global frames processed
        // This avoids using potentially huge input PTS and keeps output framerate consistent
        ctx->enc_frame->pts = vid->frames_processed + i;

        // Send to encoder
        ret = avcodec_send_frame(ctx->enc_codec_ctx, ctx->enc_frame);
        if(ret < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            loge("Error sending frame to encoder: %s", err);
            continue;
        }

        // Receive and write all packets
        while(ret >= 0)
        {
            ret = avcodec_receive_packet(ctx->enc_codec_ctx, ctx->enc_pkt);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if(ret < 0)
            {
                char err[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err, sizeof(err));
                loge("Error encoding frame: %s", err);
                break;
            }

            ctx->enc_pkt->stream_index = ctx->enc_stream->index;

            // Rescale to output stream timebase
            av_packet_rescale_ts(ctx->enc_pkt, ctx->enc_codec_ctx->time_base,
                                 ctx->enc_stream->time_base);

            av_interleaved_write_frame(ctx->enc_fmt_ctx, ctx->enc_pkt);
            av_packet_unref(ctx->enc_pkt);
        }
    }

    // Update global frames processed for next chunk
    vid->frames_processed += vid->frame_count;

    return true;

}


ListArray video_get_detect_frames(Video *vid, f32 smoothing_window)
{
    s32 skip_frames = MAX(1, (s32)(smoothing_window * vid->fps));
    logv("Number of skip frames: %d (Based on %f smoothing window)", skip_frames, smoothing_window);

    List detect_frames = list_create(vid->arena, sizeof(u32));
    u32 detect_frames_count = 0;
    u32 counter = 0;

    for(;;)
    {
        list_add(&detect_frames, (void *)&counter);

         // always want to evaluate final frame
        s32 frame_advance = MIN(skip_frames, vid->frame_count - counter - 1);
        if(frame_advance <= 0)
            break;

        counter += frame_advance;
    }

    // Discontinuities
    Temp scratch = scratch_begin();

    // determine any video discontinuities
    // ref: https://www-nlpir.nist.gov/projects/tvpubs/tvpapers03/ramonlull.paper.pdf
    logv("Determining video discontinuities...");

    u32 prev_histogram[4096] = {0};
    u32 curr_histogram[4096] = {0};
    u64 *diff_histogram = (u64 *)PUSH_ARRAY(scratch.arena, u64, vid->frame_count);

    // get color histogram for each frame of video
    // and compute a absolute difference in histogram values (summed)
    // for all video frames
    for(s32 i = 0; i < vid->frame_count; ++i)
    {
        s32 icurr = (u64)i*vid->w*vid->h*3;
        u8 *bcurr = (u8 *)&vid->data[icurr];

        if(i > 0)
        {
            // copy curr histogram to prev
            MemoryCopy(prev_histogram, curr_histogram, sizeof(u32)*4096);
            MemoryZero(curr_histogram, sizeof(u32)*4096);
        }

        // go through each pixel and compute a difference image
        for(s32 j = 0; j < vid->w*vid->h; ++j)
        {
            s32 idx = j*3;

            u8 r_curr = bcurr[idx+0];
            u8 g_curr = bcurr[idx+1];
            u8 b_curr = bcurr[idx+2];

            // get upper 4 bits from R,G,B channels and
            // combine into a 12-bit number (4096 possible values) (stored in u16)
            // this is the index into the histogram
            u16 color_bucket = ((u16)(r_curr & 0xF0) << 4) | (u16)(g_curr & 0xF0) | (u16)(b_curr & 0x0F);
            curr_histogram[MIN(color_bucket, 4095)]++;
        }

        u64 sum_histogram = 0;
        for(u32 j = 0; j < 4096; ++j)
        {
            sum_histogram += ABS((s64)curr_histogram[j] - prev_histogram[j]);
        }

        diff_histogram[i] = sum_histogram;
    }

    // 4096 buckets that sum up to vid->w * vid->h
    // A maximum difference between frames is vid->w * vid->h
    // perhaps a reasonable change from frame to frame would be 15% changed
    // to consider it a frame that should be scheduled for detection

    u64 threshold = (u64)((vid->w * vid->h) * 0.15f);

    for(u32 i = 1; i < vid->frame_count; ++i) // don't consider first frame
    {
        if(diff_histogram[i] >= threshold)
        {
            // found a frame with substantial pixel color difference from previous frame
            b32 frame_1_in_array = false;
            b32 frame_2_in_array = false;

            for(u32 j = 0; j < detect_frames.count; ++j)
            {
                u32 *frame = (u32 *)list_get(&detect_frames, j);
                if(*frame == i-1) frame_1_in_array = true;
                if(*frame == i)   frame_2_in_array = true;
            }

            // make sure both the frame with the discontinuity and prior frame are marked
            // to avoid lerping across discontinuity
            if(!frame_1_in_array)
            {
                u32 pi = i-1;
                logv("Adding discontinuity frame at %d", pi);
                list_add(&detect_frames, (void *)&pi);
            }

            if(!frame_2_in_array)
            {
                logv("Adding discontinuity frame at %d", i);
                list_add(&detect_frames, (void *)&i);
            }
        }
    }

    scratch_end(scratch);

    return list_to_array(&detect_frames);
}

void video_end(Video *vid)
{
    VideoContext *ctx = &vid->context;

    if(ctx->codec_ctx)     avcodec_free_context(&ctx->codec_ctx);
    if(ctx->frame)         av_frame_free(&ctx->frame);
    if(ctx->rgb_frame)     av_frame_free(&ctx->rgb_frame);
    if(ctx->pkt)           av_packet_free(&ctx->pkt);
    if(ctx->enc_sws_ctx)   sws_freeContext(ctx->enc_sws_ctx);
    if(ctx->enc_frame)     av_frame_free(&ctx->enc_frame);
    if(ctx->enc_pkt)       av_packet_free(&ctx->enc_pkt);
    if(ctx->enc_codec_ctx) avcodec_free_context(&ctx->enc_codec_ctx);

    if(ctx->fmt_ctx)
    {
        if(ctx->fmt_ctx->oformat && !(ctx->fmt_ctx->oformat->flags & AVFMT_NOFILE) && ctx->fmt_ctx->pb)
        {
            avio_close(ctx->fmt_ctx->pb);
        }
        avformat_free_context(ctx->fmt_ctx);
    }

    if(ctx->enc_fmt_ctx)
    {
        if(!(ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE) && ctx->enc_fmt_ctx->pb)
        {
            avio_close(ctx->enc_fmt_ctx->pb);
        }
        avformat_free_context(ctx->enc_fmt_ctx);
    }
}

void video_print(Video *vid)
{
    logi("Video [%p]", vid);
    logi("  Size:        %d, %d", vid->w, vid->h);
    logi("  Frame count: %ld", vid->frame_count_total);
    logi("  FPS:         %f", vid->fps);
    logi("  Rotation:    %d", vid->rotation);
    logi("  Codec:       %s (%d)", avcodec_get_name(vid->context.codec->id), vid->context.codec->id);
}

void video_set_log_level(LogLevel level)
{
    switch(level)
    {
        case LOG_LEVEL_QUIET:   av_log_set_level(AV_LOG_QUIET);   break;
        case LOG_LEVEL_ERROR:   av_log_set_level(AV_LOG_ERROR);   break;
        case LOG_LEVEL_WARN:    av_log_set_level(AV_LOG_WARNING); break;
        case LOG_LEVEL_INFO:    av_log_set_level(AV_LOG_INFO);    break;
        case LOG_LEVEL_VERBOSE: av_log_set_level(AV_LOG_VERBOSE); break;
        case LOG_LEVEL_DEBUG:   av_log_set_level(AV_LOG_VERBOSE); break;
        default:                av_log_set_level(AV_LOG_INFO);    break;
    }
}
