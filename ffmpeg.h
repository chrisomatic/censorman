#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <stdio.h>

void process_frame(AVFrame *pFrameRGB, int width, int height, int frame_number) {
    // Example processing: just print the frame number
    printf("Processing frame %d (size: %dx%d)\n", frame_number, width, height);
}

int ffmpeg_init()
{
    const char *filename = "images/test.mp4";
    AVFormatContext *pFormatCtx = NULL;

    av_register_all();

    // Open video file
    if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return -1;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }

    // Find the first video stream
    int videoStream = -1;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    if (videoStream == -1) {
        fprintf(stderr, "Did not find a video stream\n");
        return -1;
    }

    // Get codec parameters
    AVCodecParameters *pCodecPar = pFormatCtx->streams[videoStream]->codecpar;
    AVCodec *pCodec = avcodec_find_decoder(pCodecPar->codec_id);
    if (!pCodec) {
        fprintf(stderr, "Unsupported codec!\n");
        return -1;
    }

    // Set up codec context
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, pCodecPar);

    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return -1;
    }

    AVFrame *pFrame = av_frame_alloc();
    AVFrame *pFrameRGB = av_frame_alloc();
    if (!pFrame || !pFrameRGB) {
        fprintf(stderr, "Could not allocate frame\n");
        return -1;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
                                            pCodecCtx->width,
                                            pCodecCtx->height, 1);
    uint8_t *buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer,
                         AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);

    struct SwsContext *sws_ctx = sws_getContext(
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL, NULL, NULL
    );

    AVPacket *packet = av_packet_alloc();
    int response;
    int frame_number = 0;

    // Read frames
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        if (packet->stream_index == videoStream) {
            response = avcodec_send_packet(pCodecCtx, packet);
            if (response < 0) {
                fprintf(stderr, "Error sending packet for decoding\n");
                break;
            }

            while (response >= 0) {
                response = avcodec_receive_frame(pCodecCtx, pFrame);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
                    break;
                else if (response < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    return -1;
                }

                // Convert frame to RGB
                sws_scale(sws_ctx,
                          (uint8_t const * const *)pFrame->data,
                          pFrame->linesize,
                          0,
                          pCodecCtx->height,
                          pFrameRGB->data,
                          pFrameRGB->linesize);

                // Process the RGB frame
                process_frame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, frame_number++);
            }
        }
        av_packet_unref(packet);
    }

    // Cleanup
    av_free(buffer);
    av_frame_free(&pFrameRGB);
    av_frame_free(&pFrame);
    avcodec_free_context(&pCodecCtx);
    avformat_close_input(&pFormatCtx);
    av_packet_free(&packet);

    return 0;
}
