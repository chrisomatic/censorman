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
    for(int i = 0; i < image->w*image->h; ++i)
    {
        int n = i*image->n;
        u8 temp = image->data[n+0];
        image->data[n+0] = image->data[n+2]; // R -> B
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
        memcpy(curr+i*n, &r, 3);
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

void transform_rotate_rgb24(const uint8_t *src, uint8_t *dst, int width, int height, int rotation)
{
    int row, col;
    int dst_width = width;
    int dst_height = height;

    if (rotation == 90 || rotation == 270)
    {
        dst_width = height;
        dst_height = width;
    }

    for(int y = 0; y < height; ++y)
    {
        for(int x = 0; x < width; ++x)
        {
            const uint8_t *p = src + (y * width + x) * 3;
            uint8_t *q;

            switch(rotation) {
                case 0:
                    q = dst + (y * width + x) * 3;
                    break;
                case 270:
                    q = dst + (x * dst_width + (dst_width - 1 - y)) * 3;
                    break;
                case 180:
                    q = dst + ((dst_height - 1 - y) * width + (width - 1 - x)) * 3;
                    break;
                case 90:
                    q = dst + ((dst_height - 1 - x) * dst_width + y) * 3;
                    break;
                default:
                    // invalid rotation; fallback to no rotation
                    q = dst + (y * width + x) * 3;
                    break;
            }

            // copy RGB triplet
            q[0] = p[0];
            q[1] = p[1];
            q[2] = p[2];
        }
    }
}

static inline void blend_pixel(Image* img, int px, int py, Color c, float opacity)
{
    if (px < 0 || py < 0 || px >= img->w || py >= img->h) return;

    u8* dst = img->data + py * img->step + px * img->n;

    // Blend with existing pixel
    float alpha = (c.a / 255.0f) * opacity;
    dst[0] = (u8)((1 - alpha) * dst[0] + alpha * c.r);
    dst[1] = (u8)((1 - alpha) * dst[1] + alpha * c.g);
    dst[2] = (u8)((1 - alpha) * dst[2] + alpha * c.b);
}

static inline void draw_vline(Image* img, int x, int y1, int y2, Color c, float opacity) {

    if (x < 0 || x >= img->w) return;
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
    if (y1 < 0) y1 = 0;
    if (y2 >= img->h) y2 = img->h - 1;

    for (int y = y1; y <= y2; y++) {
        blend_pixel(img, x, y, c, opacity);
    }
}

void transform_draw_circle(Image* image, u16 x, u16 y, u16 radius, Color c, bool filled, float opacity)
{
    if (!image || radius == 0) return;

    int cx = x;
    int cy = y;
    int r = radius;

    int dx = r;
    int dy = 0;
    int err = 1 - dx;
    
    // clamp opacity
    if(opacity < 0.0f) opacity = 0.0f;
    if(opacity > 1.0f) opacity = 1.0f;

    while (dx >= dy)
    {
        if (filled)
        {
            draw_vline(image, cx + dx, cy - dy, cy + dy, c, opacity);
            draw_vline(image, cx - dx, cy - dy, cy + dy, c, opacity);
            draw_vline(image, cx + dy, cy - dx, cy + dx, c, opacity);
            draw_vline(image, cx - dy, cy - dx, cy + dx, c, opacity);
        }
        else
        {
            // Outline only
            blend_pixel(image, cx + dx, cy + dy, c, opacity);
            blend_pixel(image, cx - dx, cy + dy, c, opacity);
            blend_pixel(image, cx + dx, cy - dy, c, opacity);
            blend_pixel(image, cx - dx, cy - dy, c, opacity);
            blend_pixel(image, cx + dy, cy + dx, c, opacity);
            blend_pixel(image, cx - dy, cy + dx, c, opacity);
            blend_pixel(image, cx + dy, cy - dx, c, opacity);
            blend_pixel(image, cx - dy, cy - dx, c, opacity);
        }

        dy++;
        if (err < 0) {
            err += 2 * dy + 1;
        } else {
            dx--;
            err += 2 * (dy - dx + 1);
        }
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

    if(r.x + r.w + block_size > image->w) r.w = image->w - r.x - block_size -1;
    if(r.y + r.h + block_size > image->h) r.h = image->h - r.y - block_size -1;

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

void lanczos_downscale_rotate(Image *in, Image *out, int a)
{
    double x_scale, y_scale;

    // Compute scales depending on rotation
    switch (out->rotation)
    {
        case 90:
        case 270:
            x_scale = (double)in->w / out->h;
            y_scale = (double)in->h / out->w;
            break;
        default: // 0° or 180°
            x_scale = (double)in->w / out->w;
            y_scale = (double)in->h / out->h;
            break;
    }

    for (int oy = 0; oy < out->h; ++oy)
    {
        for (int ox = 0; ox < out->w; ++ox)
        {
            double fx, fy;

            // Map output pixel (ox,oy) -> source coordinates (fx,fy)
            switch (out->rotation)
            {
                case 90:
                    fx = (in->w - 1) - oy * x_scale;
                    fy = ox * y_scale;
                    break;
                case 180:
                    fx = (in->w - 1) - ox * x_scale;
                    fy = (in->h - 1) - oy * y_scale;
                    break;
                case 270:
                    fx = oy * x_scale;
                    fy = (in->h - 1) - ox * y_scale;
                    break;
                default: // 0°
                    fx = ox * x_scale;
                    fy = oy * y_scale;
                    break;
            }

            // Lanczos accumulation
            int x_start = floor(fx - a);
            int x_end   = floor(fx + a);
            int y_start = floor(fy - a);
            int y_end   = floor(fy + a);

            double sum_r = 0.0, sum_g = 0.0, sum_b = 0.0;
            double sum_w = 0.0;

            for (int sy = y_start; sy <= y_end; ++sy)
            {
                int cy = sy < 0 ? 0 : (sy >= in->h ? in->h-1 : sy);
                double wy = fast_lanczos((fy - sy) / y_scale, a);

                for (int sx = x_start; sx <= x_end; ++sx)
                {
                    int cx = sx < 0 ? 0 : (sx >= in->w ? in->w-1 : sx);
                    double wx = fast_lanczos((fx - sx) / x_scale, a);
                    double w = wx * wy;
                    if (w == 0.0) continue;

                    int src_idx = (cy * in->w + cx) * in->n;
                    sum_r += in->data[src_idx + 0] * w;
                    sum_g += in->data[src_idx + 1] * w;
                    sum_b += in->data[src_idx + 2] * w;
                    sum_w += w;
                }
            }

            int dst_idx = (oy * out->w + ox) * out->n;
            if (sum_w > 0.0)
            {
                out->data[dst_idx + 0] = (uint8_t)(sum_r / sum_w + 0.5);
                out->data[dst_idx + 1] = (uint8_t)(sum_g / sum_w + 0.5);
                out->data[dst_idx + 2] = (uint8_t)(sum_b / sum_w + 0.5);
            }
            else
            {
                out->data[dst_idx + 0] = 0;
                out->data[dst_idx + 1] = 0;
                out->data[dst_idx + 2] = 0;
            }
        }
    }
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

bool transform_downscale(Arena* arena, Image* source, Image* result, int scaled_size, int rotation)
{
    bool use_scaled_image = source->w > scaled_size || source->h > scaled_size;

    if(use_scaled_image)
    {
        const int a = 2; // number of iterations
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

        if(rotation == 90 || rotation == 270)
        {
            // swap the height and width
            int tmp = width_scaled;
            width_scaled = height_scaled;
            height_scaled = tmp;
        }

        result->w = width_scaled;
        result->h = height_scaled;
        result->n = source->n;
        result->step = width_scaled*result->n;
        result->rotation = rotation;
        result->arena = source->arena;
        result->frame_number = source->frame_number;
        result->detect_buffer = source->detect_buffer;

        int buffer_size = width_scaled*height_scaled*result->n;

        if(arena == NULL)
        {
            result->data = (u8*)malloc(buffer_size);
        }
        else
        {
            result->data = (u8*)arena_alloc(arena, buffer_size);
        }

        lanczos_downscale_rotate(source, result, a);
    }

    return use_scaled_image;
}

// Generate 1D Gaussian kernel
static void generate_kernel(float sigma, float **kernel, int *k_size)
{
    int radius = (int)ceilf(3 * sigma);
    *k_size = 2 * radius + 1;
    *kernel = (float *)malloc((*k_size) * sizeof(float));

    float sum = 0.0f;
    for (int i = 0; i < *k_size; i++) {
        int x = i - radius;
        (*kernel)[i] = expf(-(x * x) / (2 * sigma * sigma));
        sum += (*kernel)[i];
    }
    for (int i = 0; i < *k_size; i++) {
        (*kernel)[i] /= sum;
    }
}

// Convolution pass in horizontal or vertical direction, restricted to ROI
static void convolve_roi(Image *src, Image *dst,Rect *roi, float *kernel, int k_size,int horizontal)
{
    int radius = k_size / 2;

    for (int y = roi->y; y < roi->y + roi->h; ++y)
    {
        u8 *src_row = src->data + y * src->step;
        u8 *dst_row = dst->data + y * dst->step;

        for (int x = roi->x; x < roi->x + roi->w; ++x)
        {
            for (int c = 0; c < src->n; c++)
            {
                float sum = 0.0f;

                for (int k = -radius; k <= radius; ++k)
                {
                    int xx = x + (horizontal ? k : 0);
                    int yy = y + (horizontal ? 0 : k);

                    // clamp to border of the ROI (or image if you prefer)
                    if (xx < roi->x) xx = roi->x;
                    if (xx >= roi->x + roi->w) xx = roi->x + roi->w - 1;
                    if (yy < roi->y) yy = roi->y;
                    if (yy >= roi->y + roi->h) yy = roi->y + roi->h - 1;

                    u8 *p = src->data + yy * src->step + xx * src->n + c;
                    sum += (*p) * kernel[k + radius];
                }

                dst_row[x * src->n + c] = (u8)fminf(fmaxf(sum, 0.0f), 255.0f);
            }
        }
    }
}

void transform_gaussian_blur(Image *image, Rect *r)
{
    if (!image || !r) return;
    if (r->x >= image->w || r->y >= image->h) return;

    // Clamp ROI inside image
    if (r->x + r->w > image->w) r->w = image->w - r->x;
    if (r->y + r->h > image->h) r->h = image->h - r->y;

    float base = (r->w < r->h ? r->w : r->h);
    float sigma = 0.24 * settings.blur_strength * base;   // tune multiplier to taste

    if (sigma < 0.7f) sigma = 0.7f;  // clamp minimum

    float *kernel;
    int k_size;
    generate_kernel(sigma, &kernel, &k_size);

    // temp image buffer for intermediate result
    Image tmp = *image;
    tmp.data = (u8 *)malloc(image->step * image->h);
    memcpy(tmp.data, image->data, image->step * image->h);

    // horizontal pass
    convolve_roi(image, &tmp, r, kernel, k_size, 1);
    // vertical pass (write back into original image buffer)
    convolve_roi(&tmp, image, r, kernel, k_size, 0);

    free(tmp.data);
    free(kernel);
}

void transform_apply(Image* image, int num_rects, Rect* rects, TransformType transform)
{
    // apply transformation
    //printf("num rects; %d\n", num_rects);
    for(int i = 0; i < num_rects; ++i)
    {
        Rect r = rects[i];
        //LOGI("Rect: [%u,%u,%u,%u] confidence: %u", r.x, r.y, r.w, r.h, r.confidence);

        switch(transform)
        {
            case TRANSFORM_TYPE_BLACKOUT:       transform_draw_rect(image, r,(Color){0,0,0,255}, true, 1.0); break;
            case TRANSFORM_TYPE_PIXELATE:       transform_pixelate(image, r, settings.block_scale); break;
            case TRANSFORM_TYPE_SCRAMBLE:       transform_scramble(image, r, 0);    break;
            case TRANSFORM_TYPE_SCRAMBLE_FIXED: transform_scramble(image, r, 409);  break; // @TODO: seed
            case TRANSFORM_TYPE_BLUR:           transform_gaussian_blur(image, &r); break;
            case TRANSFORM_TYPE_TEXTURE: {
               if(settings.has_texture) {
                   transform_stretch_image(image, &texture_image, r);
               }
            }break;
            default: break;
        }
    }
}

