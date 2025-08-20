#include "base.h"

Color get_blended_color(u8* data, Color c, float opacity)
{
    u8 r = data[0];
    u8 g = data[1];
    u8 b = data[2];

    Color ret_color = {0};

    ret_color.r = opacity*c.r + (1.0 - opacity)*r;
    ret_color.g = opacity*c.g + (1.0 - opacity)*g;
    ret_color.b = opacity*c.b + (1.0 - opacity)*b;

    return ret_color;
}

float calc_iou(Rect* a, Rect* b)
{
    u16 inter_x1 = MAX(a->x, b->x);
    u16 inter_y1 = MAX(a->y, b->y);
    u16 inter_x2 = MIN(a->x + a->w, b->x + b->w);
    u16 inter_y2 = MIN(a->y + a->h, b->y + b->h);

    u16 inter_width = MAX(0, inter_x2 - inter_x1);
    u16 inter_height = MAX(0, inter_y2 - inter_y1);
    u16 inter_area = inter_width * inter_height;

    int area1 = a->w * a->h;
    int area2 = b->w * b->h;

    int union_area = area1 + area2 - inter_area;

    if (union_area == 0) return 0.0;

    return inter_area / (float)union_area;
}

void transform_draw_rect(Image* image, Rect r, Color c, bool filled, float opacity)
{
    u8* start = &image->data[r.y*image->w*image->n + r.x*image->n];
    u8* curr = start;

    int n = image->n;
    int step = image->w*n;

    // draw first line
    for(int i = 0; i < r.w; ++i)
    {
        Color r = opacity == 1.0 ? c : get_blended_color(curr+i*n,c,opacity);
        memcpy(curr + i*n, &r, 3);
    }

    curr += step;

    if(filled)
    {
        for(int j = 0; j < r.h-1; ++j)
        {
            for(int i = 0; i < r.w; ++i)
            {
                Color r = opacity == 1.0 ? c : get_blended_color(curr+i*n,c,opacity);
                memcpy(curr+i*n, &r, 3);
            }
            curr += step;
        }
    }
    else
    {
        for(int i = 0; i < r.h-1; ++i)
        {
            Color cl = opacity == 1.0 ? c : get_blended_color(curr,c,opacity);
            Color cr = opacity == 1.0 ? c : get_blended_color(curr+r.w*n,c,opacity);

            memcpy(curr,&cl, 3);         // left pixel
            memcpy(curr + r.w*n,&cr, 3); // right pixel

            curr += step;
        }
    }

    for(int i = 0; i < r.w; ++i)
    {
        Color r = opacity == 1.0 ? c : get_blended_color(curr+i*n,c,opacity);
        memcpy(curr + i*n, &r, 3);
    }
}

void transform_pixelate(Image* image, Rect r, float block_scale)
{
    u8* start = &image->data[r.y*image->w*image->n + r.x*image->n];
    u8* curr = start;

    int n = image->n;
    int step = image->w*n;

    int block_size_x = (int)(r.w * block_scale);
    int block_size_y = (int)(r.h * block_scale);
    int block_size = MIN(block_size_x, block_size_y);

    if(block_size == 0 || block_size == 1)
        return; // block_size match to pixel size

    int total_block_size = block_size * block_size;

    float avg_r = 0.0;
    float avg_g = 0.0;
    float avg_b = 0.0;

    int num_blocks_x = ceil(r.w / (float)block_size);
    int num_blocks_y = ceil(r.h / (float)block_size);

    for(int y = 0; y < num_blocks_y; ++y)
    {
        for(int x = 0; x < num_blocks_x; ++x)
        {
            avg_r = 0.0;
            avg_g = 0.0;
            avg_b = 0.0;

            curr = start + y*block_size*step + x*block_size*n;
            for(int j = 0; j < block_size; ++j)
            {
                for(int i = 0; i < block_size; ++i)
                {
                    avg_r += curr[i*n+0];
                    avg_g += curr[i*n+1];
                    avg_b += curr[i*n+2];
                }
                curr += step;
            }

            avg_r /= total_block_size;
            avg_g /= total_block_size;
            avg_b /= total_block_size;

            Color sc = {(u8)avg_r, (u8)avg_g, (u8)avg_b};

            // apply avgcolor to range
            curr = start + y*block_size*step + x*block_size*n;
            for(int j = 0; j < block_size; ++j)
            {
                for(int i = 0; i < block_size; ++i)
                {
                    memcpy(curr+i*n, &sc, 3);
                }
                curr += step;
            }
        }
    }
}

// gaussian blur

typedef enum {
    BORDER_EXTEND,
    BORDER_MIRROR,
    BORDER_CROP,
    BORDER_WRAP
} BorderPolicy;

// Compute box radii from sigma and number of passes (Ivan Kutskir method)
static inline void compute_box_radii(int *boxes, float sigma, int n) {
    float wi = sqrtf((12.0f * sigma * sigma / n) + 1.0f);
    int wl = (int)floorf(wi);
    if (wl % 2 == 0) wl--;
    int wu = wl + 2;
    float mi = (12.0f * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) /
               (float)(-4 * wl - 4);
    int m = (int)(mi + 0.5f);
    for (int i = 0; i < n; i++) {
        boxes[i] = ((i < m ? wl : wu) - 1) / 2;
    }
}

static inline int remap_index(int begin, int end, int idx, BorderPolicy p) {
    int len = end - begin;
    if (idx >= begin && idx < end) return idx;
    switch (p) {
    case BORDER_WRAP: {
        int v = (idx - begin) % len;
        if (v < 0) v += len;
        return begin + v;
    }
    case BORDER_MIRROR: {
        int off = idx - begin;
        if (off < 0) off = -off - 1;
        int period = off / len;
        int m = off % len;
        if (period % 2) return begin + (len - 1 - m);
        else return begin + m;
    }
    case BORDER_EXTEND:
        if (idx < begin) return begin;
        if (idx >= end) return end - 1;
        return idx;
    case BORDER_CROP:
    default:
        return -1; // indicates invalid, to be skipped
    }
}

static void horizontal_blur_c(const float *in, float *out, int w, int h, int channels,
                              int r, BorderPolicy p) {
    float iarr = 1.0f / (r + r + 1);
    for (int y = 0; y < h; y++) {
        int row = y * w;
        for (int c = 0; c < channels; c++) {
            float acc = 0.0f;
            for (int dx = -r; dx <= r; dx++) {
                int x0 = remap_index(0, w, dx, p);
                acc += (x0 >= 0) ? in[(row + x0) * channels + c] : 0;
            }
            for (int x = 0; x < w; x++) {
                out[(row + x) * channels + c] = acc * iarr;
                // Slide window
                int x_out = remap_index(0, w, x - r, p);
                int x_in = remap_index(0, w, x + r + 1, p);
                float val_out = (x_out >= 0) ? in[(row + x_out) * channels + c] : 0;
                float val_in = (x_in >= 0) ? in[(row + x_in) * channels + c] : 0;
                acc += (val_in - val_out);
            }
        }
    }
}

static void vertical_blur_c(const float *in, float *out, int w, int h, int channels,
                            int r, BorderPolicy p) {
    float iarr = 1.0f / (r + r + 1);
    for (int x = 0; x < w; x++) {
        for (int c = 0; c < channels; c++) {
            float acc = 0.0f;
            for (int dy = -r; dy <= r; dy++) {
                int y0 = remap_index(0, h, dy, p);
                acc += (y0 >= 0) ? in[(y0 * w + x) * channels + c] : 0;
            }
            for (int y = 0; y < h; y++) {
                out[(y * w + x) * channels + c] = acc * iarr;
                int y_out = remap_index(0, h, y - r, p);
                int y_in = remap_index(0, h, y + r + 1, p);
                float val_out = (y_out >= 0) ? in[(y_out * w + x) * channels + c] : 0;
                float val_in = (y_in >= 0) ? in[(y_in * w + x) * channels + c] : 0;
                acc += (val_in - val_out);
            }
        }
    }
}

// Public API: blur with N passes (horizontal + vertical each)
static inline void fast_gaussian_blur_c(const float *in, float *out,
                                        int w, int h, int channels,
                                        float sigma, int passes,
                                        BorderPolicy p) {
    if (passes < 1) passes = 1;
    int *boxes = (int *)malloc(passes * sizeof(int));
    if (!boxes) return;
    compute_box_radii(boxes, sigma, passes);

    float *temp = (float *)malloc(w * h * channels * sizeof(float));
    if (!temp) { free(boxes); return; }

    const float *src = in;
    float *dst = temp;

    for (int i = 0; i < passes; i++) {
        horizontal_blur_c(src, dst, w, h, channels, boxes[i], p);
        vertical_blur_c(dst, dst /* in-place vertical */, w, h, channels, boxes[i], p);
        src = dst;
    }

    // If result not in 'out', copy
    if (src != out) {
        size_t sz = (size_t)w * h * channels * sizeof(float);
        memcpy(out, src, sz);
    }

    free(boxes);
    free(temp);
}

