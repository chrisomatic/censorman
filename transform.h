#include "base.h"

void transform_draw_rect(Image* image, Rect r, Color c, bool filled)
{
    int step = image->w*image->n;

    // draw first line
    for(int k = r.x; k < r.x + r.w; ++k)
    {
        image->data[k*image->n+0] = c.r;
        image->data[k*image->n+1] = c.g;
        image->data[k*image->n+2] = c.b;
    }

    for(int j = r.y; j < r.y + r.h; ++j)
    {
        if(filled)
        {
            for(int k = r.x; k < r.x + r.w; ++k)
            {
                int kn = k*image->n;

                image->data[j*step+kn+0] = 0x00;
                image->data[j*step+kn+1] = 0x00;
                image->data[j*step+kn+2] = 0x00;
            }
        }
        else
        {
            // left pixel
            int idx = r.x*image->n + j*step;
            image->data[idx+0] = c.r;
            image->data[idx+1] = c.g;
            image->data[idx+2] = c.b;
            
            // right pixel
            image->data[j*step+r.w+0] = c.r;
            image->data[j*step+r.w+1] = c.g;
            image->data[j*step+r.w+2] = c.b;
        }
    }
    
    // draw last line
    for(int k = r.x; k < r.x + r.w; ++k)
    {
        image->data[(r.y+r.h)*step + k*image->n+0] = c.r;
        image->data[(r.y+r.h)*step + k*image->n+1] = c.g;
        image->data[(r.y+r.h)*step + k*image->n+2] = c.b;
    }
}

void transform_black_out(Image* image, Rect r)
{
    int step = image->w*image->n;

    for(int j = r.y; j < r.y + r.h; ++j)
    {
        for(int k = r.x; k < r.x + r.w; ++k)
        {
            int kn = k*image->n;

            image->data[j*step+kn+0] = 0x00;
            image->data[j*step+kn+1] = 0x00;
            image->data[j*step+kn+2] = 0x00;
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

