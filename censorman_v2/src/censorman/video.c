Video video_nil()
{
    Video vid = {0};
    return vid;
}

Video video_begin(Arena *arena, String path, String out_path, VideoSettings *settings)
{
    Video vid = {0};
    vid.arena = arena;

    MemoryCopy(&vid.settings, settings, sizeof(VideoSettings));

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

    // find audio stream
    ctx->audio_stream_index = -1;
    for(u32 i = 0; i < ctx->fmt_ctx->nb_streams; ++i)
    {
        if(ctx->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            ctx->audio_stream_index = i;
            break;
        }
    }

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

        // Fallback: metadata "rotate" tag
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
    s32 max_frames     = floor(vid.settings.max_buffer_size / (f64)frame_rgb_size);

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

    if(vid.settings.no_encode) return vid;

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
    ctx->enc_codec_ctx->thread_count = 0; // auto
    ctx->enc_codec_ctx->thread_type  = FF_THREAD_FRAME | FF_THREAD_SLICE;

    ctx->enc_stream->time_base = ctx->enc_codec_ctx->time_base;

    if (ctx->enc_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->enc_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    av_dict_set(&ctx->enc_opts, "preset", "superfast", 0);
    av_dict_set(&ctx->enc_opts, "tune", "zerolatency", 0);

    // audio stream setup
    if(ctx->audio_stream_index >= 0)
    {
        ctx->enc_audio_stream = avformat_new_stream(ctx->enc_fmt_ctx, NULL);

        if(vid.settings.distort_audio)
        {
            // decode/encode path
            ctx->audio_dec_codec = avcodec_find_decoder(
                ctx->fmt_ctx->streams[ctx->audio_stream_index]->codecpar->codec_id);
            ctx->audio_dec_ctx = avcodec_alloc_context3(ctx->audio_dec_codec);
            avcodec_parameters_to_context(ctx->audio_dec_ctx,
                ctx->fmt_ctx->streams[ctx->audio_stream_index]->codecpar);
            avcodec_open2(ctx->audio_dec_ctx, ctx->audio_dec_codec, NULL);

            ctx->audio_enc_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
            ctx->audio_enc_ctx   = avcodec_alloc_context3(ctx->audio_enc_codec);
            ctx->audio_enc_ctx->sample_rate = ctx->audio_dec_ctx->sample_rate;
            av_channel_layout_copy(&ctx->audio_enc_ctx->ch_layout, &ctx->audio_dec_ctx->ch_layout);
            ctx->audio_enc_ctx->sample_fmt  = ctx->audio_enc_codec->sample_fmts[0];
            ctx->audio_enc_ctx->bit_rate    = 128000;
            ctx->audio_enc_ctx->time_base   = (AVRational){1, ctx->audio_dec_ctx->sample_rate};

            if(ctx->enc_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                ctx->audio_enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

            avcodec_open2(ctx->audio_enc_ctx, ctx->audio_enc_codec, NULL);

            avcodec_parameters_from_context(ctx->enc_audio_stream->codecpar, ctx->audio_enc_ctx);
            ctx->enc_audio_stream->time_base = ctx->audio_enc_ctx->time_base;

            ctx->audio_frame   = av_frame_alloc();
            ctx->audio_enc_pkt = av_packet_alloc();

            // swr: decoded format -> encoder format
            swr_alloc_set_opts2(&ctx->swr_ctx,
                &ctx->audio_enc_ctx->ch_layout, ctx->audio_enc_ctx->sample_fmt, ctx->audio_enc_ctx->sample_rate,
                &ctx->audio_dec_ctx->ch_layout, ctx->audio_dec_ctx->sample_fmt, ctx->audio_dec_ctx->sample_rate,
                0, NULL);
            swr_init(ctx->swr_ctx);
        }
        else
        {
            // passthrough path
            avcodec_parameters_copy(ctx->enc_audio_stream->codecpar,
                ctx->fmt_ctx->streams[ctx->audio_stream_index]->codecpar);
            ctx->enc_audio_stream->codecpar->codec_tag = 0;
            ctx->enc_audio_stream->time_base =
                ctx->fmt_ctx->streams[ctx->audio_stream_index]->time_base;
        }

        ctx->audio_packet_max   = 4096;
        ctx->audio_packets      = (AVPacket **)malloc(sizeof(AVPacket*) * ctx->audio_packet_max);
        ctx->audio_packet_count = 0;
    }

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
    
    // write rotation to encoder stream
    char *rotate_str = cstring_from_s64(arena, vid.rotation);
    av_dict_set(&ctx->enc_stream->metadata, "rotate", rotate_str, 0);
    
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
    av_frame_get_buffer(ctx->enc_frame, 0);

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
    b32 eof_reached = false;
    b32 hit_max_buffer = false;

    AVFrame  *frame = ctx->frame;
    AVPacket *pkt   = ctx->pkt;

    while(frame_count < vid->frame_count_max)
    {
        ret = av_read_frame(ctx->fmt_ctx, pkt);
        if(ret < 0)
        {
            eof_reached = true;
            break;
        }

        if(pkt->stream_index != ctx->stream_index)
        {
            // capture audio packets
            if(pkt->stream_index == ctx->audio_stream_index &&
               ctx->audio_packets &&
               ctx->audio_packet_count < ctx->audio_packet_max)
            {
                ctx->audio_packets[ctx->audio_packet_count++] = av_packet_clone(pkt);
            }
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

            u8 *dp = (u8 *)vid->data + (u64)frame_count * frame_rgb_size;
            u8 *dest_data[4]    = { dp, NULL, NULL, NULL };
            s32 dest_linesize[4] = { rgb_stride, 0, 0, 0 };
            sws_scale(ctx->sws_ctx, (const u8 *const*)frame->data, frame->linesize,
                      0, height, dest_data, dest_linesize);

            vid->pts_buffer[frame_count] = frame->pts;
            frame_count++;
            if(frame_count >= vid->frame_count_max)
            {
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
            u8 *dest_data[4]    = { dp, NULL, NULL, NULL };
            s32 dest_linesize[4] = { rgb_stride, 0, 0, 0 };
            sws_scale(ctx->sws_ctx, (const u8 *const*)frame->data, frame->linesize,
                      0, height, dest_data, dest_linesize);

            vid->pts_buffer[frame_count] = frame->pts;
            frame_count++;
        }
    }

    logv("Loaded %d frames", frame_count);

    vid->frame_count   = frame_count;
    vid->load_complete = !hit_max_buffer || eof_reached;

    return (frame_count > 0);
}

b32 video_save_frames(Video *vid)
{
    if(!vid->data || vid->frame_count == 0)
        return false;

    VideoContext *ctx = &vid->context;

    s32 width          = vid->w;
    s32 height         = vid->h;
    s32 rgb_stride     = width * 3;
    s32 frame_rgb_size = rgb_stride * height;
    s32 ret;

    for(u32 i = 0; i < vid->frame_count; ++i)
    {
        av_frame_make_writable(ctx->enc_frame);

        ctx->enc_frame_src->data[0]     = ((u8 *)vid->data) + ((u64)i * frame_rgb_size);
        ctx->enc_frame_src->linesize[0] = rgb_stride;

        ret = sws_scale_frame(ctx->enc_sws_ctx, ctx->enc_frame, ctx->enc_frame_src);
        if(ret < 0)
        {
            logw("Error scaling frame %d (ret = 0x%d)", i, ret);
            continue;
        }

        ctx->enc_frame->pts = vid->frames_processed + i;

        ret = avcodec_send_frame(ctx->enc_codec_ctx, ctx->enc_frame);
        if(ret < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            loge("Error sending frame to encoder: %s", err);
            continue;
        }

        while(ret >= 0)
        {
            ret = avcodec_receive_packet(ctx->enc_codec_ctx, ctx->enc_pkt);
            if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
            else if(ret < 0)
            {
                char err[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, err, sizeof(err));
                loge("Error encoding frame: %s", err);
                break;
            }

            ctx->enc_pkt->stream_index = ctx->enc_stream->index;
            av_packet_rescale_ts(ctx->enc_pkt, ctx->enc_codec_ctx->time_base,
                                 ctx->enc_stream->time_base);
            av_interleaved_write_frame(ctx->enc_fmt_ctx, ctx->enc_pkt);
            av_packet_unref(ctx->enc_pkt);
        }
    }

    // audio
    if(ctx->audio_stream_index >= 0 && ctx->enc_audio_stream)
    {
        AVStream *in_audio = ctx->fmt_ctx->streams[ctx->audio_stream_index];

        if(vid->settings.distort_audio)
        {
            static u64 sample_pos = 0;

            for(u32 i = 0; i < ctx->audio_packet_count; ++i)
            {
                AVPacket *apkt = ctx->audio_packets[i];

                ret = avcodec_send_packet(ctx->audio_dec_ctx, apkt);
                av_packet_free(&ctx->audio_packets[i]);
                ctx->audio_packets[i] = NULL;
                if(ret < 0) continue;

                while(avcodec_receive_frame(ctx->audio_dec_ctx, ctx->audio_frame) == 0)
                {
                    // convert decoded frame to float planar for ring mod
                    AVFrame *proc_frame     = av_frame_alloc();
                    proc_frame->format      = AV_SAMPLE_FMT_FLTP;
                    proc_frame->sample_rate = ctx->audio_frame->sample_rate;
                    proc_frame->nb_samples  = ctx->audio_frame->nb_samples;
                    av_channel_layout_copy(&proc_frame->ch_layout, &ctx->audio_frame->ch_layout);
                    av_frame_get_buffer(proc_frame, 0);

                    SwrContext *to_float = NULL;
                    swr_alloc_set_opts2(&to_float,
                        &proc_frame->ch_layout, AV_SAMPLE_FMT_FLTP, proc_frame->sample_rate,
                        &ctx->audio_frame->ch_layout, (enum AVSampleFormat)ctx->audio_frame->format,
                        ctx->audio_frame->sample_rate, 0, NULL);
                    swr_init(to_float);
                    swr_convert(to_float,
                        proc_frame->data, proc_frame->nb_samples,
                        (const u8**)ctx->audio_frame->data, ctx->audio_frame->nb_samples);
                    swr_free(&to_float);

                    // ring modulation: multiply each sample by sin(2π * carrier * t)
                    s32 num_channels = proc_frame->ch_layout.nb_channels;
                    f32 carrier      = vid->settings.distort_audio_carrier_hz;
                    f32 sample_rate  = (f32)proc_frame->sample_rate;

                    for(s32 ch = 0; ch < num_channels; ++ch)
                    {
                        f32 *samples = (f32 *)proc_frame->data[ch];
                        for(s32 s = 0; s < proc_frame->nb_samples; ++s)
                        {
                            f32 t = (f32)(sample_pos + s) / sample_rate;
                            samples[s] *= sinf(TAU * carrier * t);
                            samples[s] = tanhf(samples[s] * 3.0f);
                        }
                    }
                    sample_pos += proc_frame->nb_samples;

                    // convert float planar -> encoder sample format
                    AVFrame *enc_frame     = av_frame_alloc();
                    enc_frame->format      = ctx->audio_enc_ctx->sample_fmt;
                    enc_frame->sample_rate = ctx->audio_enc_ctx->sample_rate;
                    enc_frame->nb_samples  = proc_frame->nb_samples;
                    av_channel_layout_copy(&enc_frame->ch_layout, &ctx->audio_enc_ctx->ch_layout);
                    av_frame_get_buffer(enc_frame, 0);

                    swr_convert(ctx->swr_ctx,
                        enc_frame->data, enc_frame->nb_samples,
                        (const u8**)proc_frame->data, proc_frame->nb_samples);

                    av_frame_free(&proc_frame);

                    enc_frame->pts = av_rescale_q(
                        ctx->audio_frame->pts,
                        ctx->audio_dec_ctx->time_base,
                        ctx->audio_enc_ctx->time_base);

                    avcodec_send_frame(ctx->audio_enc_ctx, enc_frame);
                    av_frame_free(&enc_frame);

                    while(avcodec_receive_packet(ctx->audio_enc_ctx, ctx->audio_enc_pkt) == 0)
                    {
                        ctx->audio_enc_pkt->stream_index = ctx->enc_audio_stream->index;
                        av_packet_rescale_ts(ctx->audio_enc_pkt,
                            ctx->audio_enc_ctx->time_base,
                            ctx->enc_audio_stream->time_base);
                        av_interleaved_write_frame(ctx->enc_fmt_ctx, ctx->audio_enc_pkt);
                        av_packet_unref(ctx->audio_enc_pkt);
                    }
                }
            }
        }
        else
        {
            // passthrough
            for(u32 i = 0; i < ctx->audio_packet_count; ++i)
            {
                AVPacket *apkt = ctx->audio_packets[i];
                apkt->stream_index = ctx->enc_audio_stream->index;
                av_packet_rescale_ts(apkt, in_audio->time_base,
                                     ctx->enc_audio_stream->time_base);
                av_interleaved_write_frame(ctx->enc_fmt_ctx, apkt);
                av_packet_free(&ctx->audio_packets[i]);
                ctx->audio_packets[i] = NULL;
            }
        }

        ctx->audio_packet_count = 0;
    }

    vid->frames_processed += vid->frame_count;

    return true;
}

void video_save_done(Video *vid)
{
    VideoContext *vid_ctx = &vid->context;

    // flush video encoder
    s32 ret = avcodec_send_frame(vid_ctx->enc_codec_ctx, NULL);
    while(ret >= 0)
    {
        ret = avcodec_receive_packet(vid_ctx->enc_codec_ctx, vid_ctx->enc_pkt);
        if(ret == AVERROR(EAGAIN))
            continue;
        else if(ret == AVERROR_EOF)
            break;
        else if(ret < 0)
        {
            char err[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err, sizeof(err));
            loge("Error flushing encoder: %s", err);
            break;
        }

        vid_ctx->enc_pkt->stream_index = vid_ctx->enc_stream->index;
        av_packet_rescale_ts(vid_ctx->enc_pkt, vid_ctx->enc_codec_ctx->time_base,
                             vid_ctx->enc_stream->time_base);
        av_interleaved_write_frame(vid_ctx->enc_fmt_ctx, vid_ctx->enc_pkt);
        av_packet_unref(vid_ctx->enc_pkt);
    }

    // flush audio encoder if distorting
    if(vid->settings.distort_audio && vid_ctx->audio_enc_ctx)
    {
        avcodec_send_frame(vid_ctx->audio_enc_ctx, NULL);
        while(avcodec_receive_packet(vid_ctx->audio_enc_ctx, vid_ctx->audio_enc_pkt) == 0)
        {
            vid_ctx->audio_enc_pkt->stream_index = vid_ctx->enc_audio_stream->index;
            av_packet_rescale_ts(vid_ctx->audio_enc_pkt,
                vid_ctx->audio_enc_ctx->time_base,
                vid_ctx->enc_audio_stream->time_base);
            av_interleaved_write_frame(vid_ctx->enc_fmt_ctx, vid_ctx->audio_enc_pkt);
            av_packet_unref(vid_ctx->audio_enc_pkt);
        }
    }

    av_write_trailer(vid_ctx->enc_fmt_ctx);

    if(!(vid_ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE) && vid_ctx->enc_fmt_ctx->pb)
        avio_flush(vid_ctx->enc_fmt_ctx->pb);
}

ListArray video_get_detect_frames(Video *vid, f32 smoothing_window)
{
    s32 skip_frames = MAX(1, (s32)(smoothing_window * vid->fps));
    logv("Number of skip frames: %d (Based on %f smoothing window)", skip_frames, smoothing_window);

    List detect_frames = list_create(vid->arena, sizeof(u32));
    u32 counter = 0;

    for(;;)
    {
        list_add(&detect_frames, (void *)&counter);

        s32 frame_advance = MIN(skip_frames, vid->frame_count - counter - 1);
        if(frame_advance <= 0)
            break;

        counter += frame_advance;
    }

    Temp scratch = scratch_begin();

    logv("Determining video discontinuities...");

    u32 prev_histogram[4096] = {0};
    u32 curr_histogram[4096] = {0};
    u64 *diff_histogram = (u64 *)PUSH_ARRAY(scratch.arena, u64, vid->frame_count);

    for(s32 i = 0; i < vid->frame_count; ++i)
    {
        s32 icurr      = (u64)i * vid->w * vid->h;
        RGBColor *bcurr = &vid->data[icurr];

        if(i > 0)
        {
            MemoryCopy(prev_histogram, curr_histogram, sizeof(u32) * 4096);
            MemoryZero(curr_histogram, sizeof(u32) * 4096);
        }

        for(s32 j = 0; j < vid->w * vid->h; ++j)
        {
            RGBColor curr    = bcurr[j];
            u16 color_bucket = ((u16)(curr.r & 0xF0) << 4) | (u16)(curr.g & 0xF0) | ((u16)(curr.b & 0xF0) >> 4);
            curr_histogram[MIN(color_bucket, 4095)]++;
        }

        u64 sum_histogram = 0;
        for(u32 j = 0; j < 4096; ++j)
            sum_histogram += ABS((s64)curr_histogram[j] - prev_histogram[j]);

        diff_histogram[i] = sum_histogram;
    }

    u64 threshold = (u64)((vid->w * vid->h) * 0.10);

    for(u32 i = 1; i < vid->frame_count; ++i)
    {
        if(diff_histogram[i] >= threshold)
        {
            b32 frame_1_in_array = false;
            b32 frame_2_in_array = false;

            for(u32 j = 0; j < detect_frames.count; ++j)
            {
                u32 *frame = (u32 *)list_get(&detect_frames, j);
                if(*frame == i-1) frame_1_in_array = true;
                if(*frame == i)   frame_2_in_array = true;
            }

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

    ListArray arr = list_to_array(&detect_frames);
    list_array_sort(&arr, list_compare_fn_s32_asc);

    return arr;
}

void video_end(Video *vid)
{
    VideoContext *ctx = &vid->context;

    if(ctx->codec_ctx)     avcodec_free_context(&ctx->codec_ctx);
    if(ctx->frame)         av_frame_free(&ctx->frame);
    if(ctx->rgb_frame)     av_frame_free(&ctx->rgb_frame);
    if(ctx->pkt)           av_packet_free(&ctx->pkt);
    if(ctx->sws_ctx)       sws_freeContext(ctx->sws_ctx);
    if(ctx->enc_sws_ctx)   sws_freeContext(ctx->enc_sws_ctx);
    if(ctx->enc_frame)     av_frame_free(&ctx->enc_frame);
    if(ctx->enc_frame_src) av_frame_free(&ctx->enc_frame_src);
    if(ctx->enc_pkt)       av_packet_free(&ctx->enc_pkt);
    if(ctx->enc_codec_ctx) avcodec_free_context(&ctx->enc_codec_ctx);

    // audio cleanup
    if(ctx->audio_packets)
    {
        for(u32 i = 0; i < ctx->audio_packet_count; ++i)
            if(ctx->audio_packets[i]) av_packet_free(&ctx->audio_packets[i]);
        free(ctx->audio_packets);
        ctx->audio_packets = NULL;
    }

    if(ctx->audio_dec_ctx)  avcodec_free_context(&ctx->audio_dec_ctx);
    if(ctx->audio_enc_ctx)  avcodec_free_context(&ctx->audio_enc_ctx);
    if(ctx->audio_frame)    av_frame_free(&ctx->audio_frame);
    if(ctx->audio_enc_pkt)  av_packet_free(&ctx->audio_enc_pkt);
    if(ctx->swr_ctx)        swr_free(&ctx->swr_ctx);

    if(ctx->fmt_ctx)
    {
        if(ctx->fmt_ctx->oformat && !(ctx->fmt_ctx->oformat->flags & AVFMT_NOFILE) && ctx->fmt_ctx->pb)
            avio_close(ctx->fmt_ctx->pb);
        avformat_free_context(ctx->fmt_ctx);
    }

    if(ctx->enc_fmt_ctx)
    {
        if(!(ctx->enc_fmt_ctx->oformat->flags & AVFMT_NOFILE) && ctx->enc_fmt_ctx->pb)
            avio_close(ctx->enc_fmt_ctx->pb);
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
