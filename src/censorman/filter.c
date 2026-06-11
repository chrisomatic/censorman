#include "font_8x16.h"

//===================================
// Static prototypes
//===================================

static void     blend_color_in_image(Image *img, s64 x, s64 y, RGBColor color, f32 factor);
static void     draw_vline(Image* image, s64 x, s64 y1, s64 y2, RGBColor color, f32 opacity);
static void     draw_box(Image *image, Box *box, RGBColor color, b32 filled, f32 opacity, u32 dash_cadence);
static void     draw_circle(Image *image, u32 x, u32 y, u32 radius, RGBColor color, b32 filled, f32 opacity);
static void     convolve(Image *src, Image *dst, Box *roi, f32 *kernel, s32 k_size, bool horizontal);
static void     draw_string(Image* image, s64 x, s64 y, RGBColor color, String str);
static void     draw_char(Image* image, u8 c, u16 x, u16 y, RGBColor color);
static void     put_pixel(Image *image, s64 x, s64 y, RGBColor color);
static RGBColor get_pixel(Image *image, s64 x, s64 y);
static RGBColor blend_color(RGBColor base, RGBColor color, f32 factor);

//===================================
// Global vars
//===================================

Image g_texture_image;

//===================================
// Filter functions
//===================================

void filter_apply(Filter filter, Image *image, Box *box)
{
    if(image->props.w == 0 || image->props.h == 0)
        return;
    
    stopwatch_begin(image->stopwatch, S(__func__));

    Image temp_image = {0};
    Box   temp_box   = {0};

    Image *image_ = image;
    Box   *box_   = box;

    if(filter.elliptical)
    {
        // If we want an ellipse cutout of the filter
        // We need to copy the box region to a temp image buffer
        // And update the box dimensions first

        s32 max_dim = MAX(box->w, box->h);

        temp_image.data = PUSH_ARRAY(image->arena, RGBColor, box->w * box->h);
        temp_image.props.w = box->w;
        temp_image.props.h = box->h;
        temp_image.props.rotation = image->props.rotation;
        temp_image.props.scale = 1.0f;

        image_copy_rect(image, box->x, box->y, &temp_image, 0,0, box->w, box->h);
        MemoryCopyStruct(&temp_box, box);

        temp_box.x = 0;
        temp_box.y = 0;

        image_ = &temp_image;
        box_   = &temp_box;
    }
    else
    {
        // Clamp box since we are simply axis-aligned rectangular
        *box_ = box_clamp(*box_, &image_->props);
    }

    switch(filter.type)
    {
        case FILTER_TYPE_BLACKOUT:
            filter_blackout(image_, box_);
            break;
        case FILTER_TYPE_BLUR_BOX:
            filter_blur_box(image_, box_, filter.param);
            break;
        case FILTER_TYPE_BLUR_GAUSSIAN:
            filter_blur_gaussian(image_, box_, filter.param);
            break;
        case FILTER_TYPE_PIXELATE:
            filter_pixelate(image_, box_, filter.param);
            break;
        case FILTER_TYPE_SCRAMBLE:
            filter_scramble(image_, box_);
            break;
        case FILTER_TYPE_TEXTURE:
            filter_texture(image_, box_);
            break;
        case FILTER_TYPE_NONE:
        default:
            break;
    }

    if(filter.elliptical)
    {
        // Now we need to copy the temp image
        // Back to the original image, but ignore 
        // the pixels that are outside of the rounded+rotated ellipse

        f32 h = box->w/2.0f;
        f32 k = box->h/2.0f;

        f32 a = box->w/2.0f;
        f32 b = box->h/2.0f;

        // get angle between eyes
        Point eye_left  = box->landmarks[0];
        Point eye_right = box->landmarks[1];

        f32 eye_angle = RAD((f32)image->props.rotation);

        f32 eye_dist = vec2_distance(
                VEC2(eye_left.x, eye_left.y), 
                VEC2(eye_right.x, eye_right.y)
        );

        if(eye_dist > 20.0f)
        {
            // only consider eye angle if the eyes are at least 20px apart
            eye_angle += atanf((eye_right.y - eye_left.y)/(f32)(eye_right.x - eye_left.x));
        }

        // filter out pixels if they fall outside of rounded+rotated ellipse
        for(s64 j = 0; j < box->h; ++j)
        {
            for(s64 i = 0; i < box->w; ++i)
            {
                s64 curr_src_x = i;
                s64 curr_src_y = j;

                s64 curr_dst_x = box->x + i;
                s64 curr_dst_y = box->y + j;

                b32 outside_of_src_range = (curr_src_x < 0 || curr_src_x >= temp_image.props.w) || (curr_src_y < 0 || curr_src_y >= temp_image.props.h);
                b32 outside_of_dst_range = (curr_dst_x < 0 || curr_dst_x >= image->props.w) || (curr_dst_y < 0 || curr_dst_y >= image->props.h);

                if(outside_of_src_range || outside_of_dst_range)
                {
                    // pixel outside of destination range,
                    // ignore
                    continue;
                }

                // calculate ellipse value
                // for this x,y
                // |((x-h)cos(t)+(y-k)sin(t))/a|^r + |((x-h)sin(t)-(y-k)cos(t))/b|^r <= 1

                f32 x = i;
                f32 y = j;

                f32 va = ABS(((x-h)*cosf(eye_angle) + (y-k)*sinf(eye_angle))/a);
                f32 vb = ABS(((x-h)*sinf(eye_angle) - (y-k)*cosf(eye_angle))/b);
                f32 v = (va*va*va) + (vb*vb*vb); // r = 3

                b32 include = (v <= 1.0f);

                if(include)
                {
                    RGBColor *src_pixel = temp_image.data + (curr_src_y * temp_image.props.w) + curr_src_x;
                    RGBColor *dst_pixel = image->data + (curr_dst_y * image->props.w) + curr_dst_x;

                    MemoryCopyStruct(dst_pixel, src_pixel);
                }
            }
        }
    }

    stopwatch_end(image->stopwatch, S(__func__));
}

void filter_blackout(Image *image, Box *box)
{
    RGBColor black = (RGBColor){0,0,0};
    draw_box(image, box, black, true, 1.0, 0);
}

void filter_pixelate(Image* image, Box *box, f32 block_scale)
{
    RGBColor* limit = &image->data[image->props.w*image->props.h - 1];
    RGBColor* start = &image->data[box->y*image->props.w + box->x];
    RGBColor* curr = start;

    s32 block_size = MAX(box->w, box->h)*block_scale;

    if(block_size == 0 || block_size == 1)
        return; // block_size matches pixel

    s32 total_block_size = block_size * block_size;

    f64 avg_r = 0.0;
    f64 avg_g = 0.0;
    f64 avg_b = 0.0;

    s32 block_size_x = block_size;
    s32 block_size_y = block_size;

    s32 num_blocks_x = block_size_x > 0 ? ceil((f32)box->w / block_size_x) : 0;
    s32 num_blocks_y = block_size_y > 0 ? ceil((f32)box->h / block_size_y) : 0;

    s32 leftover_x = block_size_x > 0 ? box->w % block_size_x : 0;
    s32 leftover_y = block_size_y > 0 ? box->h % block_size_y : 0;

    for(s32 y = 0; y < num_blocks_y; ++y)
    {
        for(s32 x = 0; x < num_blocks_x; ++x)
        {
            avg_r = 0.0;
            avg_g = 0.0;
            avg_b = 0.0;

            curr = start + y*block_size_y*image->props.w + x*block_size_x;

            for(s32 j = 0; j < block_size_y; ++j)
            {
                if(curr > limit)
                {
                    logw("Hit end of image!");
                    break;
                }

                for(s32 i = 0; i < block_size_x; ++i)
                {
                    avg_r += curr[i].r;
                    avg_g += curr[i].g;
                    avg_b += curr[i].b;
                }

                curr += MIN(image->props.w, limit - curr);
            }

            avg_r /= total_block_size;
            avg_g /= total_block_size;
            avg_b /= total_block_size;

            RGBColor sc = {(u8)avg_r, (u8)avg_g, (u8)avg_b};

            // offsets to deal with truncated blocks at edges of box
            s32 adj_block_size_x = (x == num_blocks_x - 1 && leftover_x > 0) ? leftover_x : block_size_x;
            s32 adj_block_size_y = (y == num_blocks_y - 1 && leftover_y > 0) ? leftover_y : block_size_y;

            // apply avgcolor to range
            curr = start + y*block_size_y*image->props.w + x*block_size_x;

            for(s32 j = 0; j < adj_block_size_y; ++j)
            {
                for(s32 i = 0; i < adj_block_size_x; ++i)
                {
                    MemoryCopy(curr+i, &sc, sizeof(RGBColor));
                }
                curr += image->props.w;
            }
        }
    }
}

void filter_scramble(Image *image, Box *box)
{
    Temp scratch = scratch_begin();

    RGBColor* start = &image->data[box->y*image->props.w + box->x];

    // initialize unprocessed list
    s64 num_pixels = box->w * box->h;
    s64 *unprocessed = (s64 *)PUSH_ARRAY(scratch.arena, s64, num_pixels);
    s64 unprocessed_count = num_pixels;

    for(s64 i = 0; i < num_pixels; ++i)
        unprocessed[i] = i;

    for(;;)
    {
        if(unprocessed_count <= 1)
            break;

        s64 idx1 = (randgen_u32() % unprocessed_count);
        s64 idx2 = (randgen_u32() % unprocessed_count);

        // swap two pixels
        s64 u1 = unprocessed[idx1];
        s64 u2 = unprocessed[idx2];

        s64 offset1 = image->props.w*(u1/box->w) + (u1%box->w);
        s64 offset2 = image->props.w*(u2/box->w) + (u2%box->w);

        RGBColor tmp = {0};
        RGBColor *a = start+offset1;
        RGBColor *b = start+offset2;

        // Swap pixels
        tmp = *a;
        *a = *b;
        *b = tmp;

        // remove both indices from unprocessed
        memcpy(&unprocessed[idx1],&unprocessed[unprocessed_count-1], sizeof(s64));
        unprocessed_count--;

        memcpy(&unprocessed[idx2],&unprocessed[unprocessed_count-1], sizeof(s64));
        unprocessed_count--;
    }

    scratch_end(scratch);

}

void filter_blur_gaussian(Image *image, Box *box, f32 blur_strength)
{
    Temp scratch = scratch_begin();

    f32 base = (box->w < box->h ? box->w : box->h);
    f32 sigma = MAX(0.70, 0.24 * blur_strength * base);
    s32 radius = (s32)ceilf(3 * sigma);
    s32 k_size = 2 * radius + 1;
    f32 *kernel = PUSH_ARRAY(scratch.arena, f32, k_size);

    f32 sum = 0.0;
    for (s32 i = 0; i < k_size; ++i)
    {
        s32 x = i - radius;
        kernel[i] = expf(-(x*x) / (2*sigma*sigma));
        sum += kernel[i];
    }

    for (s32 i = 0; i < k_size; ++i)
    {
        kernel[i] /= sum;
    }

    // temp image buffer for intermediate result
    Image tmp = *image;
    tmp.data = PUSH_ARRAY(scratch.arena, RGBColor, image->props.w * image->props.h);
    MemoryCopy(tmp.data, image->data, image->props.w * image->props.h * sizeof(RGBColor));

    // horizontal pass
    convolve(image, &tmp, box, kernel, k_size, true);

    // vertical pass (write back into original image buffer)
    convolve(&tmp, image, box, kernel, k_size, false);

    scratch_end(scratch);
}

void filter_blur_box(Image *image, Box *box, f32 blur_strength)
{
    Temp scratch = scratch_begin();

    box->w = CLAMP(box->w, 1, image->props.w);
    box->h = CLAMP(box->h, 1, image->props.h);

    // calculate fitting radius based on box size
    f32 longest_dimension = (f32)(box->w >= box->h ? box->w : box->h);
    s32 radius = (s32)(0.24 * blur_strength * longest_dimension);
    radius = MAX(radius, 1);

    s32 k_size = 2 * radius + 1;
    f32 norm = 1.0 / k_size;

    // Precompute clamped indices for horizontal and vertical passes
    s32 *h_indices = PUSH_ARRAY(scratch.arena, s32, box->w + 2 * radius);
    s32 *v_indices = PUSH_ARRAY(scratch.arena, s32, box->h + 2 * radius);

    for(s64 i = -radius; i < box->w + radius; ++i)
    {
        s32 idx = box->x + i;
        if(idx < box->x) idx = box->x;
        if(idx >= box->x + box->w) idx = box->x + box->w - 1;
        h_indices[i + radius] = idx;
    }

    for(s64 i = -radius; i < box->h + radius; ++i)
    {
        s32 idx = box->y + i;
        if(idx < box->y) idx = box->y;
        if(idx >= box->y + box->h) idx = box->y + box->h - 1;
        v_indices[i + radius] = idx;
    }

    RGBColor *p_buffer = PUSH_ARRAY(scratch.arena, RGBColor, image->props.w*image->props.h);

    for(s32 pass = 0; pass < 3; ++pass)
    {
        // horizontal
        for(s32 y = box->y; y < box->y + box->h; ++y)
        {
            RGBColor *src_row = image->data + y * image->props.w;
            RGBColor *dst_row = p_buffer + y * image->props.w;

            s64 sum_r = 0;
            s64 sum_g = 0;
            s64 sum_b = 0;

            // Initialize sum for first pixel
            for(s32 k = 0; k < k_size; ++k)
            {
                RGBColor pixel = src_row[h_indices[k]];

                sum_r += pixel.r;
                sum_g += pixel.g;
                sum_b += pixel.b;
            }

            RGBColor *dst_pixel;

            dst_pixel = &dst_row[box->x];

            // write dest pixel
            dst_pixel->r = (u8)(sum_r * norm);
            dst_pixel->g = (u8)(sum_g * norm);
            dst_pixel->b = (u8)(sum_b * norm);

            for(s32 x = box->x + 1; x < box->x + box->w; ++x)
            {
                s32 left  = h_indices[x - box->x - 1 + 0]; // previous left
                s32 right = h_indices[x - box->x + k_size - 1]; // new right

                RGBColor right_pixel = src_row[right];
                RGBColor left_pixel  = src_row[left];
                
                sum_r += right_pixel.r - left_pixel.r;
                sum_g += right_pixel.g - left_pixel.g;
                sum_b += right_pixel.b - left_pixel.b;

                dst_pixel = &dst_row[x];

                // write dest pixel
                dst_pixel->r = (u8)(sum_r * norm);
                dst_pixel->g = (u8)(sum_g * norm);
                dst_pixel->b = (u8)(sum_b * norm);
            }
        }

        // vertical
        for(s32 x = box->x; x < box->x + box->w; ++x)
        {
            s64 sum_r = 0;
            s64 sum_g = 0;
            s64 sum_b = 0;

            // Initialize sum for first pixel
            for(s32 k = 0; k < k_size; ++k)
            {
                RGBColor pixel = p_buffer[v_indices[k] * image->props.w + x];
                sum_r += pixel.r;
                sum_g += pixel.g;
                sum_b += pixel.b;
            }

            RGBColor *dst_row;
            dst_row = image->data + box->y * image->props.w;

            RGBColor *dst_pixel;
            dst_pixel = &dst_row[x];

            // write 
            dst_pixel->r = (u8)(sum_r * norm);
            dst_pixel->g = (u8)(sum_g * norm);
            dst_pixel->b = (u8)(sum_b * norm);

            for(s32 y = box->y + 1; y < box->y + box->h; ++y)
            {
                s32 top    = v_indices[y - box->y - 1 + 0];
                s32 bottom = v_indices[y - box->y + k_size - 1];

                RGBColor top_pixel    = p_buffer[top * image->props.w + x];
                RGBColor bottom_pixel = p_buffer[bottom * image->props.w + x];

                sum_r += bottom_pixel.r - top_pixel.r;
                sum_g += bottom_pixel.g - top_pixel.g;
                sum_b += bottom_pixel.b - top_pixel.b;

                dst_row = image->data + y * image->props.w;

                dst_pixel = &dst_row[x];
                dst_pixel->r = (u8)(sum_r * norm);
                dst_pixel->g = (u8)(sum_g * norm);
                dst_pixel->b = (u8)(sum_b * norm);
            }
        }

        if (pass < 2)
        {
            MemoryCopy(p_buffer, image->data, image->props.w * image->props.h * sizeof(RGBColor));
        }
    }

    scratch_end(scratch);
}

void filter_texture(Image *image, Box *box)
{
    if(image_is_empty(&g_texture_image))
        return;

    f32 scale_x = (f32)g_texture_image.props.w / box->w;
    f32 scale_y = (f32)g_texture_image.props.h / box->h;

    for(s32 dy = 0; dy < box->h; ++dy)
    {
        for(s32 dx = 0; dx < box->w; ++dx)
        {
            s32 sx = (s32)(dx * scale_x);
            s32 sy = (s32)(dy * scale_y);

            if(sx >= g_texture_image.props.w) sx = g_texture_image.props.w - 1;
            if(sy >= g_texture_image.props.h) sy = g_texture_image.props.h - 1;

            RGBColor *src_pixel = g_texture_image.data + sy * g_texture_image.props.w + sx;
            RGBColor *dst_pixel = image->data + (box->y + dy) * image->props.w + (box->x + dx);

            // check for magenta pixel (poor man's transparency)
            b32 ignore = (src_pixel->r == 0xFF && src_pixel->g == 0x00 && src_pixel->b == 0xFF);

            if(ignore)
                continue;

            // Copy pixel data
            dst_pixel->r = src_pixel->r;
            dst_pixel->g = src_pixel->g;
            dst_pixel->b = src_pixel->b;
        }
    }
}

void filter_draw_debug_info(Image *image, BoxFrame *box_frame, f32 box_padding, b32 no_labels)
{
    if(image_is_empty(image))
        return;

    String label = string_format(image->arena, "%000d %s",
            box_frame->frame_number, box_frame->interpolated ? "interpolated" : "");

    draw_string(image, 2, image->props.h - 18, (RGBColor){200,200,0}, label);

    for(s64 j = 0; j < box_frame->box_count; ++j)
    {
        Box *box = &box_frame->boxes[j];

        // make sure box is clamped
        *box = box_clamp(*box, &image->props);

        RGBColor color_list[LANDMARK_COUNT] = {0};

        color_list[0] = (RGBColor){255,0,0};   // red
        color_list[1] = (RGBColor){0,255,0};   // green
        color_list[2] = (RGBColor){0,0,255};   // blue
        color_list[3] = (RGBColor){255,255,0}; // yellow
        color_list[4] = (RGBColor){255,0,255}; // magenta

        RGBColor color_bad  = (RGBColor){255,0,0};
        RGBColor color_good = (RGBColor){0,255,0};
        RGBColor color      = blend_color(color_bad, color_good, box->confidence / 100.0);

        const f32 box_transparency = 0.60f;

        // draw outline
        draw_box(image, box, color, false, box_transparency, 0);

        if(box_padding > 0.0f)
        {
            // draw box without padding
            Box unpadded = {0};

            f32 unpad_factor = 1.0f / (1.0f+box_padding);

            unpadded.w = (s32)box->w * unpad_factor;
            unpadded.h = (s32)box->h * unpad_factor;

            s32 pad_x = (s32)((box->w - unpadded.w)/2.0f);
            s32 pad_y = (s32)((box->h - unpadded.h)/2.0f);

            unpadded.x = box->x + pad_x;
            unpadded.y = box->y + pad_y;

            unpadded.x = CLAMP(unpadded.x, 0, (s32)(image->props.w - 1));
            unpadded.y = CLAMP(unpadded.y, 0, (s32)(image->props.h - 1));
            unpadded.w = CLAMP(unpadded.w, 1, (s32)(image->props.w - unpadded.x - 1));
            unpadded.h = CLAMP(unpadded.h, 1, (s32)(image->props.h - unpadded.y - 1));

            draw_box(image, &unpadded, (RGBColor){0,200,200}, false, box_transparency, 5);
        }

        // draw confidence string
        draw_string(image, box->x+2, box->y+2, color, string_format(image->arena, "%u", box->confidence));

        if(!no_labels)
        {
            Box label_box = {0};

            if(box->y > FONT_HEIGHT + 1)
            {
                label_box.x = box->x;
                label_box.y = box->y - FONT_HEIGHT - 1;
                label_box.w = box->w;
                label_box.h = FONT_HEIGHT + 1;
            }
            else if(box->y + box->w < image->props.h - FONT_HEIGHT - 2)
            {
                label_box.x = box->x;
                label_box.y = box->y + box->h;
                label_box.w = box->w;
                label_box.h = FONT_HEIGHT + 1;
            }

            // draw label
            draw_box(image, &label_box, color, true, box_transparency, 0);
            draw_string(image, label_box.x + 1, label_box.y + 1, (RGBColor){32,32,32}, detect_type_to_string(box->type));
        }

        u32 radius = MAX(1, box->h * 0.015);
        for(s64 i = 0; i < LANDMARK_COUNT; ++i)
        {
            Point p = box->landmarks[i];
            draw_circle(image, p.x, p.y, radius, color_list[i], true, box_transparency);
        }
    }
}

String filter_to_string(FilterType type)
{
    switch(type)
    {
        case FILTER_TYPE_BLACKOUT:      return S("blackout");
        case FILTER_TYPE_BLUR_BOX:      return S("box_blur");
        case FILTER_TYPE_BLUR_GAUSSIAN: return S("gaussian_blur");
        case FILTER_TYPE_PIXELATE:      return S("pixelate");
        case FILTER_TYPE_SCRAMBLE:      return S("scramble");
        case FILTER_TYPE_TEXTURE:       return S("texture");
        case FILTER_TYPE_NONE:
        default: break;
            
    }

    return S("none");
}

FilterType filter_from_string(String str)
{
    if(string_equal(str, S("blackout")))
        return FILTER_TYPE_BLACKOUT;
    
    if(string_equal(str, S("blur")) || string_equal(str, S("box_blur")))
        return FILTER_TYPE_BLUR_BOX;

    if(string_equal(str, S("gaussian_blur")))
        return FILTER_TYPE_BLUR_GAUSSIAN;

    if(string_equal(str, S("pixelate")))
        return FILTER_TYPE_PIXELATE;

    if(string_equal(str, S("scramble")))
        return FILTER_TYPE_SCRAMBLE;

    if(string_equal(str, S("texture")))
        return FILTER_TYPE_TEXTURE;

    return FILTER_TYPE_NONE;
}

FacialFeature facial_feature_from_string(String str)
{
    if(string_equal(str, S("eyes")))
        return FACIAL_FEATURE_EYES;

    if(string_equal(str, S("nose")))
        return FACIAL_FEATURE_NOSE;

    if(string_equal(str, S("mouth")))
        return FACIAL_FEATURE_MOUTH;

    if(string_equal(str, S("cheeks")))
        return FACIAL_FEATURE_CHEEKS;

    if(string_equal(str, S("forehead")))
        return FACIAL_FEATURE_FOREHEAD;

    return FACIAL_FEATURE_NONE;
}

//===================================
// Static functions
//===================================

static void convolve(Image *src, Image *dst, Box *roi, f32 *kernel, s32 k_size, bool horizontal)
{
    s32 radius = k_size / 2;

    for (s32 y = roi->y; y < roi->y + roi->h; ++y)
    {
        RGBColor *dst_row = dst->data + y * dst->props.w;

        for (s32 x = roi->x; x < roi->x + roi->w; ++x)
        {
            f32 sum_r = 0.0;
            f32 sum_g = 0.0;
            f32 sum_b = 0.0;

            for (s32 k = -radius; k <= radius; ++k)
            {
                s32 xx = x + (horizontal ? k : 0);
                s32 yy = y + (horizontal ? 0 : k);

                // clamp
                if(xx < roi->x) xx = roi->x;
                if(yy < roi->y) yy = roi->y;
                if(xx >= roi->x + roi->w) xx = roi->x + roi->w - 1;
                if(yy >= roi->y + roi->h) yy = roi->y + roi->h - 1;

                RGBColor *p = src->data + yy * src->props.w + xx;

                sum_r += p->r * kernel[k + radius];
                sum_g += p->g * kernel[k + radius];
                sum_b += p->b * kernel[k + radius];
            }

            RGBColor *dst_pixel = &dst_row[x];

            dst_pixel->r = (u8)CLAMP(sum_r, 0.0, 255.0f);
            dst_pixel->g = (u8)CLAMP(sum_g, 0.0, 255.0f);
            dst_pixel->b = (u8)CLAMP(sum_b, 0.0, 255.0f);
        }
    }
}

static RGBColor blend_color(RGBColor base, RGBColor color, f32 factor)
{
    factor = CLAMP(factor, 0.0, 1.0);
    if(factor == 1.0)
        return color;

    RGBColor ret = {0};

    ret.r = factor*color.r + (1.0 - factor)*base.r;
    ret.g = factor*color.g + (1.0 - factor)*base.g;
    ret.b = factor*color.b + (1.0 - factor)*base.b;

    return ret;
}

static void put_pixel(Image *image, s64 x, s64 y, RGBColor color)
{
    image->data[y*image->props.w + x] = color;
}

static RGBColor get_pixel(Image *image, s64 x, s64 y)
{
    return image->data[y*image->props.w + x];
}

static void blend_color_in_image(Image *img, s64 x, s64 y, RGBColor color, f32 factor)
{
    RGBColor *pixel = &img->data[y*img->props.w + x];
    *pixel = blend_color(*pixel, color, factor);
}

static void draw_box(Image *image, Box *box, RGBColor color, b32 filled, f32 opacity, u32 dash_cadence)
{
    Box box_clamped = {0};
    MemoryCopy(&box_clamped, box, sizeof(Box));

    // clamp box ahead of time
    box_clamped.x = CLAMP(box_clamped.x, 0, image->props.w - 1);
    box_clamped.y = CLAMP(box_clamped.y, 0, image->props.h - 1);
    box_clamped.w = CLAMP(box_clamped.w, 1, image->props.w - box_clamped.x - 1);
    box_clamped.h = CLAMP(box_clamped.h, 1, image->props.h - box_clamped.y - 1);

    RGBColor *start = &image->data[box_clamped.y*image->props.w + box_clamped.x];
    RGBColor *curr  = start;

    b32 dashing = true;

    // draw first line
    for(s32 i = 0; i < box_clamped.w; ++i)
    {
        if(dash_cadence != 0 && i > 0 && (i % dash_cadence == 0))
            dashing = !dashing;

        if(dashing)
        {
            RGBColor *r = &curr[i];
            *r = blend_color(*r, color, opacity);
        }
    }
    curr += image->props.w;

    if(filled)
    {
        for(s32 j = 0; j < box_clamped.h-1; ++j)
        {
            for(s32 i = 0; i < box_clamped.w; ++i)
            {
                RGBColor *r = &curr[i];
                *r = blend_color(*r, color, opacity);
            }
            curr += image->props.w;
        }
    }
    else
    {
        for(s32 i = 0; i < box_clamped.h-1; ++i)
        {
            if(dash_cadence != 0 && i > 0 && (i % dash_cadence == 0))
                dashing = !dashing;

            if(dashing)
            {
                RGBColor *cl = &curr[0];
                RGBColor *cr = &curr[MAX(box_clamped.w-1,0)];

                *cl = blend_color(*cl, color, opacity);
                *cr = blend_color(*cr, color, opacity);
            }

            curr += image->props.w;
        }
    }

    for(s32 i = 0; i < box_clamped.w; ++i)
    {
        if(dash_cadence != 0 && i > 0 && (i % dash_cadence == 0))
            dashing = !dashing;

        if(dashing)
        {
            RGBColor *r = &curr[i];
            *r = blend_color(*r, color, opacity);
        }
    }
}

static inline void draw_vline(Image* image, s64 x, s64 y1, s64 y2, RGBColor color, f32 opacity)
{
    if(x < 0 || x >= image->props.w)
        return;

    if(y1 > y2)
    {
        s64 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }

    if(y1 < 0) y1 = 0;
    if(y2 >= image->props.h) y2 = image->props.h - 1;

    for(s64 y = y1; y <= y2; y++)
    {
        RGBColor pixel = get_pixel(image, x, y);
        put_pixel(image, x, y, blend_color(pixel, color, opacity));
    }
}

static void draw_circle(Image *image, u32 x, u32 y, u32 radius, RGBColor color, b32 filled, f32 opacity)
{
    if(!image || radius == 0)
        return;

    s64 cx = x;
    s64 cy = y;
    s64 r = radius;

    s64 dx = r;
    s64 dy = 0;
    s64 err = 1 - dx;
    
    opacity = CLAMP(opacity, 0.0, 1.0);

    for(;;)
    {
        if(dx < dy)
            break;

        if(filled)
        {
            draw_vline(image, cx + dx, cy - dy, cy + dy, color, opacity);
            draw_vline(image, cx - dx, cy - dy, cy + dy, color, opacity);
            draw_vline(image, cx + dy, cy - dx, cy + dx, color, opacity);
            draw_vline(image, cx - dy, cy - dx, cy + dx, color, opacity);
        }
        else
        {
            // outline only
            blend_color_in_image(image, cx + dx, cy + dy, color, opacity);
            blend_color_in_image(image, cx - dx, cy + dy, color, opacity);
            blend_color_in_image(image, cx + dx, cy - dy, color, opacity);
            blend_color_in_image(image, cx - dx, cy - dy, color, opacity);
            blend_color_in_image(image, cx + dy, cy + dx, color, opacity);
            blend_color_in_image(image, cx - dy, cy + dx, color, opacity);
            blend_color_in_image(image, cx + dy, cy - dx, color, opacity);
            blend_color_in_image(image, cx - dy, cy - dx, color, opacity);
        }

        dy++;

        if(err < 0)
        {
            err += 2 * dy + 1;
        }
        else
        {
            dx--;
            err += 2 * (dy - dx + 1);
        }
    }
}

static void draw_char(Image* image, u8 c, u16 x, u16 y, RGBColor color)
{
    if(!char_is_printable(c))
        return;

    s32 index = (c - ' ');

    const u8 *glyph = font8x16[index];
    for(s32 row = 0; row < 16; row++)
    {
        u8 bits = glyph[row];
        for(s32 col = 0; col < 8; col++)
        {
            if (bits & (1 << (7 - col)))
            {
                put_pixel(image, x + col, y + row, color);
            }
        }
    }
}

static void draw_string(Image* image, s64 x, s64 y, RGBColor color, String str)
{
    s64 dx = x;
    for(int i = 0; i < str.len; ++i)
    {
        if(dx + 8 > image->props.w) break;
        draw_char(image, str.data[i], dx, y, color);
        dx += 8; // fixed width spacing
    }
}
