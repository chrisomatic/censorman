#pragma once

#include "base.h"

inline Color get_pixel(Image* image, int x, int y)
{
    Color c = {0};
    memcpy(&c, &image->data[y*image->w*image->n + x*image->n], 3);
    return c;
}

inline void reverse_rgb_order(Image *image)
{
    LOGI("Reversing RGB Order... pixel count: %d", image->w*image->h);
    for(int i = 0; i < image->w*image->h; ++i)
    {
        int n = i*image->n;
        u8 temp = image->data[n+0];
        image->data[n+0] = image->data[i*3+2]; // R -> B
        image->data[n+2] = temp;               // B -> R
    }
}

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

void transform_scramble(Image* image, Rect r, u32 seed)
{
    u8* start = &image->data[r.y*image->w*image->n + r.x*image->n];

    if(seed > 0)
    {
        // seed of 0 means "don't seed"
        srand(seed);
    }

    // initialize unprocessed list
    int num_pixels = r.w*r.h;
    int unprocessed[num_pixels] = {0};
    int unprocessed_count = num_pixels;

    for(int i = 0; i < num_pixels; ++i)
        unprocessed[i] = i;

    for(;;)
    {
        if(unprocessed_count <= 1)
            break;

        int idx1 = rand() % unprocessed_count;
        int idx2 = rand() % unprocessed_count;

        // swap two pixels

        int u1 = unprocessed[idx1];
        int u2 = unprocessed[idx2];

        int offset1 = image->w*image->n*(u1/r.w) + image->n*(u1%r.w);
        int offset2 = image->w*image->n*(u2/r.w) + image->n*(u2%r.w);

        Color tmp = {0};
        memcpy(&tmp, start+offset1, 3);
        memcpy(start+offset1,start+offset2,3);
        memcpy(start+offset2, &tmp, 3);

        // remove both indices from unprocessed
        memcpy(&unprocessed[idx1],&unprocessed[unprocessed_count-1], sizeof(int));
        unprocessed_count--;
        memcpy(&unprocessed[idx2],&unprocessed[unprocessed_count-1], sizeof(int));
        unprocessed_count--;
    }
    
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

    int block_size = MIN(r.w, r.h)*block_scale;

    if(block_size == 0 || block_size == 1)
        return; // block_size match to pixel size

    int total_block_size = block_size * block_size;

    float avg_r = 0.0;
    float avg_g = 0.0;
    float avg_b = 0.0;

    int num_blocks_x = ceil(r.w / (float)block_size);
    int num_blocks_y = ceil(r.h / (float)block_size);

    int block_size_x = block_size;
    int block_size_y = block_size;

    for(int y = 0; y < num_blocks_y; ++y)
    {
        for(int x = 0; x < num_blocks_x; ++x)
        {
            avg_r = 0.0;
            avg_g = 0.0;
            avg_b = 0.0;

            curr = start + y*block_size_y*step + x*block_size_x*n;

            for(int j = 0; j < block_size_y; ++j)
            {
                for(int i = 0; i < block_size_x; ++i)
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

            int offset_x = x == num_blocks_x - 1 ? block_size_x - (r.w % block_size_x) : 0;
            int offset_y = y == num_blocks_y - 1 ? block_size_y - (r.h % block_size_y) : 0;

            // apply avgcolor to range
            curr = start + y*block_size_y*step + x*block_size_x*n;
            for(int j = 0; j < block_size_y - offset_y; ++j)
            {
                for(int i = 0; i < block_size_x - offset_x; ++i)
                {
                    memcpy(curr+i*n, &sc, 3);
                }
                curr += step;
            }
        }
    }
}

void transform_stretch_image(Image *dst, Image *src, Rect r)
{
    // Scaling factors
    float scaleX = (float)src->w / r.w;
    float scaleY = (float)src->h / r.h;

    // Iterate through the destination rectangle
    for (int dy = 0; dy < r.h; ++dy)
    {
        for (int dx = 0; dx < r.w; ++dx)
        {
            // Compute the corresponding position in the source image
            int sx = (int)(dx * scaleX);
            int sy = (int)(dy * scaleY);

            // Ensure we're within bounds for the source image
            if (sx >= src->w) sx = src->w - 1;
            if (sy >= src->h) sy = src->h - 1;

            // Get the source pixel's starting index
            u8 *src_pixel = src->data + sy * src->step + sx * src->n;

            // Get the destination pixel's starting index
            u8 *dst_pixel = dst->data + (r.y + dy) * dst->step + (r.x + dx) * dst->n;

            // Copy pixel data (assume both images have the same number of channels)
            for (int c = 0; c < src->n; c++)
                dst_pixel[c] = src_pixel[c];
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



// Down Scaling

#define KERNEL_TABLE_SIZE 1024
float lanczos_table[KERNEL_TABLE_SIZE];
float inv_a_scale = 0.0;

// performs pre-computations to make things fast
void lanczos_init(int a) {
    for (int i = 0; i < KERNEL_TABLE_SIZE; ++i) {
        float x = ((float)i / (KERNEL_TABLE_SIZE - 1)) * a;
        if (x == 0.0)
            lanczos_table[i] = 1.0;
        else if (x < a)
            lanczos_table[i] = (sin(PI*x) / (PI*x)) * (sin(PI*x/a) / (PI*x/a));
        else
            lanczos_table[i] = 0.0;
    }

    inv_a_scale = (KERNEL_TABLE_SIZE - 1) / (float)a;
}

static inline float fast_lanczos(double x, int a) {

    x = ABSF(x);
    if (x >= a)
        return 0.0;

    int idx = (int)(x*inv_a_scale);
    return lanczos_table[idx];
}

void lanczos_downscale(Image *in, Image *out, int a)
{
    double x_scale = (double)in->w / out->w;
    double y_scale = (double)in->h / out->h;

    for (int y = 0; y < out->h; ++y)
    {
        double source_y = (y + 0.5) * y_scale;
        int y_start = floor(source_y - a);
        int y_end   = floor(source_y + a);

        for (int x = 0; x < out->w; ++x)
        {
            double source_x = (x + 0.5) * x_scale;
            int x_start = floor(source_x - a);
            int x_end   = floor(source_x + a);


            double sum_red = 0.0;
            double sum_green = 0.0;
            double sum_blue = 0.0;
            double sum_weights = 0.0;

            // Determine the contributing input pixel region based on 'a'
            // and the downscaling ratio

            for (int j = y_start; j <= y_end; ++j)
            {
                double weight_y = fast_lanczos((j - source_y) / y_scale, a);
                int clamped_j = j < 0 ? 0 : (j >= in->h ? in->h-1 : j);
                float row_off = clamped_j*in->w*in->n;

                for (int i = x_start; i <= x_end; ++i)
                {
                    // Calculate weights using the Lanczos kernel
                    double weight_x = fast_lanczos((i - source_x) / x_scale, a);
                    double weight = weight_x * weight_y;

                    int clamped_i = i < 0 ? 0 : (i >= in->w ? in->w-1 : i);
                    int offset = row_off + clamped_i*in->n;

                    sum_red   += in->data[offset+0] * weight;
                    sum_green += in->data[offset+1] * weight;
                    sum_blue  += in->data[offset+2] * weight;

                    sum_weights += weight;
                }
            }

            // Normalize and set the output pixel

            Color out_pixel;
            out_pixel.r = (u8)(sum_red / sum_weights + 0.5);
            out_pixel.g = (u8)(sum_green / sum_weights + 0.5);
            out_pixel.b = (u8)(sum_blue / sum_weights + 0.5);

            u8* curr = &out->data[y*out->w*out->n + x*out->n];
            memset(curr+0,out_pixel.r,1);
            memset(curr+1,out_pixel.g,1);
            memset(curr+2,out_pixel.b,1);
        }
    }
}

bool transform_downscale_image(Arena* arena, Image* source, Image* result, int scaled_size)
{
    bool use_scaled_image = source->w > scaled_size || source->h > scaled_size;

    if(use_scaled_image)
    {
        const int a = 2;
        lanczos_init(a);

        // downscale largest dimension 
        float aspect = source->w / (float)source->h;

        int width_scaled = 0;
        int height_scaled = 0;

        if(aspect > 1.0)
        {
            // width is larger than height (most common)
            width_scaled = scaled_size;
            height_scaled = width_scaled / aspect;
        }
        else
        {
            height_scaled = scaled_size;
            width_scaled = height_scaled * aspect;
        }

        result->w = width_scaled;
        result->h = height_scaled;
        result->n = source->n;
        result->step = width_scaled*result->n;

        if(arena == NULL)
        {

            result->data = (u8*)malloc(width_scaled*height_scaled*result->n);
        }
        else
        {
            result->data = (u8*)arena_alloc(arena, width_scaled*height_scaled*result->n);
        }

        lanczos_downscale(source, result, a);
    }

    return use_scaled_image;
}

void transform_apply(Image* image, int num_rects, Rect* rects, TransformType transform)
{
    // apply transformation
    for(int i = 0; i < num_rects; ++i)
    {
        Rect r = rects[i];
        LOGI("Rect: [%u,%u,%u,%u] confidence: %u", r.x, r.y, r.w, r.h, r.confidence);

        switch(transform)
        {
            case TRANSFORM_TYPE_BLACKOUT:       transform_draw_rect(image, r,(Color){0,0,0,255}, true, 1.0); break;
            case TRANSFORM_TYPE_PIXELATE:       transform_pixelate(image, r, settings.block_scale); break;
            case TRANSFORM_TYPE_SCRAMBLE:       transform_scramble(image, r, 0);    break;
            case TRANSFORM_TYPE_SCRAMBLE_FIXED: transform_scramble(image, r, 409);  break; // @TODO
            case TRANSFORM_TYPE_TEXTURE:        if(settings.has_texture) transform_stretch_image(image, &texture_image, r); break;
            case TRANSFORM_TYPE_BLUR: break;
            default: break;
        }
    }
}

