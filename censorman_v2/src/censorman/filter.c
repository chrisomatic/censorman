#include "font_8x16.h"

//===================================
// Static prototypes
//===================================

static void     blend_color_in_image(Image *img, s64 x, s64 y, RGBColor color, f32 factor);
static void     draw_vline(Image* image, s64 x, s64 y1, s64 y2, RGBColor color, f32 opacity);
static void     draw_box(Image *image, Box *box, RGBColor color, b32 filled, u32 border_thickness, f32 opacity);
static void     draw_circle(Image *image, u32 x, u32 y, u32 radius, RGBColor color, b32 filled, f32 opacity);
static void     convolve(Image *src, Image *dst, Box *roi, f32 *kernel, s32 k_size, bool horizontal);
static void     draw_string(Image* image, s64 x, s64 y, RGBColor color, String str);
static void     draw_char(Image* image, u8 c, u16 x, u16 y, RGBColor color);
static void     put_pixel(Image *image, s64 x, s64 y, RGBColor color);
static RGBColor get_pixel(Image *image, s64 x, s64 y);
static RGBColor blend_color(RGBColor base, RGBColor color, f32 factor);

//===================================
// Filter functions
//===================================

void filter_apply(Filter filter, Image *image, Box *box)
{
    stopwatch_begin(image->stopwatch, S(__func__));
    switch(filter.type)
    {
        case FILTER_TYPE_BLACKOUT:
            filter_blackout(image, box);
            break;
        case FILTER_TYPE_BLUR_BOX:
            break;
        case FILTER_TYPE_BLUR_GAUSSIAN:
            filter_blur_gaussian(image, box, filter.blur_strength);
            break;
        case FILTER_TYPE_PIXELATE:
            break;
        case FILTER_TYPE_TEXTURE:
            break;
        case FILTER_TYPE_NONE:
        default:

    }

    stopwatch_end(image->stopwatch, S(__func__));
}

void filter_blackout(Image *image, Box *box)
{
    RGBColor black = (RGBColor){0,0,0};
    draw_box(image, box, black, true, 1, 1.0);
}

void filter_draw_debug_info(Image *image, Box *box)
{
    RGBColor color_list[LANDMARK_COUNT] = {0};

    color_list[0] = (RGBColor){255,0,0};   // red
    color_list[1] = (RGBColor){0,255,0};   // green
    color_list[2] = (RGBColor){0,0,255};   // blue
    color_list[3] = (RGBColor){255,255,0}; // yellow
    color_list[4] = (RGBColor){255,0,255}; // magenta

    RGBColor color_bad  = (RGBColor){255,0,0};
    RGBColor color_good = (RGBColor){0,255,0};

    RGBColor color = blend_color(color_bad, color_good, box->confidence / 100.0);

    // draw outline
    draw_box(image, box, color, false, 1, 1.0);

    // draw confidence string
    draw_string(image, box->x+2, box->y+2, color, string_format(image->arena, "%u", box->confidence));

    // draw detect type
    /*
    draw_string(image, box->x+1, MAX(box->y+1, box->y+box->h-17), (RGBColor){0,0,0}, detect_type_to_string(box->type));
    draw_string(image, box->x+2, MAX(box->y+2, box->y+box->h-18), (RGBColor){200,200,0}, detect_type_to_string(box->type));
    */

    u32 radius = MAX(1, box->h / 50.0);
    for(s64 i = 0; i < LANDMARK_COUNT; ++i)
    {
        Point p = box->landmarks[i];
        draw_circle(image, p.x, p.y, radius, color_list[i], true, 1.0);
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
        case FILTER_TYPE_TEXTURE:       return S("texture");
        case FILTER_TYPE_NONE:
        default:
    }

    return S("none");
}

FilterType filter_from_string(String str)
{
    if(string_equal(str, S("blackout")))
        return FILTER_TYPE_BLACKOUT;
    
    if(string_equal(str, S("box_blur")))
        return FILTER_TYPE_BLUR_BOX;

    if(string_equal(str, S("blur")) || string_equal(str, S("gaussian_blur")))
        return FILTER_TYPE_BLUR_GAUSSIAN;

    if(string_equal(str, S("pixelate")))
        return FILTER_TYPE_PIXELATE;

    if(string_equal(str, S("texture")))
        return FILTER_TYPE_TEXTURE;

    return FILTER_TYPE_NONE;
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
    tmp.data = PUSH_ARRAY(scratch.arena, RGBColor, image->w * image->h);
    MemoryCopy(tmp.data, image->data, image->w * image->h * sizeof(RGBColor));

    // horizontal pass
    convolve(image, &tmp, box, kernel, k_size, true);

    // vertical pass (write back into original image buffer)
    convolve(&tmp, image, box, kernel, k_size, false);

    scratch_end(scratch);
}

//===================================
// Static functions
//===================================

static void convolve(Image *src, Image *dst, Box *roi, f32 *kernel, s32 k_size, bool horizontal)
{
    s32 radius = k_size / 2;

    for (s32 y = roi->y; y < roi->y + roi->h; ++y)
    {
        RGBColor *src_row = src->data + y * src->w;
        RGBColor *dst_row = dst->data + y * dst->w;

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

                RGBColor *p = src->data + yy * src->w + xx;

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
    image->data[y*image->w + x] = color;
}

static RGBColor get_pixel(Image *image, s64 x, s64 y)
{
    return image->data[y*image->w + x];
}

static void blend_color_in_image(Image *img, s64 x, s64 y, RGBColor color, f32 factor)
{
    RGBColor *pixel = &img->data[y*img->w + x];
    *pixel = blend_color(*pixel, color, factor);
}

static void draw_box(Image *image, Box *box, RGBColor color, b32 filled, u32 border_thickness, f32 opacity)
{
    RGBColor *start = &image->data[box->y*image->w + box->x];
    RGBColor *curr  = start;

    // draw first line
    for(u32 j = 0; j < border_thickness; ++j)
    {
        for(s32 i = 0; i <= box->w; ++i)
        {
            RGBColor *r = &curr[i];
            *r = blend_color(*r, color, opacity);
        }
        if(j < border_thickness - 1)
            curr += image->w;
    }

    curr += image->w;

    if(filled)
    {
        for(s32 j = 0; j < box->h-1; ++j)
        {
            for(s32 i = 0; i < box->w; ++i)
            {
                RGBColor *r = &curr[i];
                *r = blend_color(*r, color, opacity);
            }
            curr += image->w;
        }
    }
    else
    {
        for(s32 i = 0; i < box->h-1; ++i)
        {
            for(u32 j = 0; j < border_thickness; ++j)
            {
                RGBColor *cl = &curr[0 + j];
                RGBColor *cr = &curr[MAX(box->w - j,0)];

                *cl = blend_color(*cl, color, opacity);
                *cr = blend_color(*cr, color, opacity);
            }

            curr += image->w;
        }
    }

    curr -= (image->w*MAX(0,(border_thickness-1)));

    for(u32 j = 0; j < border_thickness; ++j)
    {
        for(s32 i = 0; i <= box->w; ++i)
        {
            RGBColor *r = &curr[i];
            *r = blend_color(*r, color, opacity);
        }

        if(j < border_thickness - 1)
            curr += image->w;
    }
}

static inline void draw_vline(Image* image, s64 x, s64 y1, s64 y2, RGBColor color, f32 opacity)
{
    if(x < 0 || x >= image->w)
        return;

    if(y1 > y2)
    {
        s64 tmp = y1;
        y1 = y2;
        y2 = tmp;
    }

    if(y1 < 0)         y1 = 0;
    if(y2 >= image->h) y2 = image->h - 1;

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
        if(dx + 8 > image->w) break;
        draw_char(image, str.data[i], dx, y, color);
        dx += 8; // fixed width spacing
    }
}
