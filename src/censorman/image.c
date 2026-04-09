
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "libexif/exif-data.h"

Image image_nil(void)
{
    Image image = {0};
    return image;
}

b32 image_is_empty(Image *image)
{
    return (!image->data) || (image->props.w == 0 && image->props.h == 0);
}

Image image_load(Arena *arena, String path, Stopwatch *stopwatch)
{
    Image image = {0};
    image.arena = arena;
    image.stopwatch = stopwatch;

    stopwatch_begin(image.stopwatch, S(__func__));

    Temp scratch = scratch_begin();
    char *path_cstr = string_to_cstr(scratch.arena, path);

    s32 w,h,n;
    u8* data = stbi_load(path_cstr, &w, &h, &n, 0);

    scratch_end(scratch);

    if(!data)
    {
        loge("Failed to load image: " STR_FMT, STR_ARG(path));
        return image;
    }

    if(n < 3)
    {
        loge("Not enough channels on image (n = %d)", n);
        stbi_image_free(data);
        return image;
    }

    image.props.w = w;
    image.props.h = h;
    image.data = PUSH_ARRAY(image.arena, RGBColor, w*h);
    image.props.scale = 1.0;
    image.props.rotation = image_get_rotation_from_file(path_cstr);

    // pack RGB (remove alpha channel if needed)
    for(s32 i = 0; i < w*h; ++i)
    {
        RGBColor *pixel = &image.data[i];

        pixel->r = data[i*n+0];
        pixel->g = data[i*n+1];
        pixel->b = data[i*n+2];
    }

    logv("Loaded image " STR_FMT " [w: %u h: %u]", STR_ARG(path), image.props.w,image.props.h);

    // free buffer
    stbi_image_free(data);

    stopwatch_end(image.stopwatch, S(__func__));

    MemoryCopy(&image.props_orig, &image.props, sizeof(ImageProps));
    
    return image;
}

b32 image_save(Image *image, String path)
{
    stopwatch_begin(image->stopwatch, S(__func__));

    Temp scratch = scratch_begin();

    char *output_file_cstr = string_to_cstr(scratch.arena, path);

    s32 res = stbi_write_png(output_file_cstr, image->props.w, image->props.h, 3, image->data, 3*image->props.w);

    scratch_end(scratch);

    stopwatch_end(image->stopwatch, S(__func__));

    if(res == 0)
    {
        loge("Failed to write output, path: " STR_FMT, STR_ARG(path));
        return false;
    }

    return true;
}

Image image_rotate(Image source, u32 degrees, ClockDir direction)
{
    //   0: (x,y) -> ( x, y)
    //  90: (x,y) -> ( y,-x)
    // 180: (x,y) -> (-x,-y)
    // 270: (x,y) -> (-y, x)

    if(degrees == ROTATE_0)
    {
        // no rotation, just return the source (no copy)
        return source;
    }

    stopwatch_begin(source.stopwatch, S(__func__));

    Image output = source;
    
    b32 dim_flipped = (degrees == ROTATE_90 || degrees == ROTATE_270);

    output.data = PUSH_ARRAY(source.arena, RGBColor, source.props.w * source.props.h);
    output.props.w = dim_flipped ? source.props.h : source.props.w;
    output.props.h = dim_flipped ? source.props.w : source.props.h;

    output.props.pad_x = dim_flipped ? source.props.pad_y : source.props.pad_x;
    output.props.pad_y = dim_flipped ? source.props.pad_x : source.props.pad_y;

    s32 out_x = 0;
    s32 out_y = 0;

    if(direction == CCW)
    {
        if(degrees == ROTATE_90)       degrees = ROTATE_270;
        else if(degrees == ROTATE_270) degrees = ROTATE_90;
    }

    for(int y = 0; y < source.props.h; ++y)
    {
        for(int x = 0; x < source.props.w; ++x)
        {
            switch(degrees)
            {
                case ROTATE_90:
                    out_x = source.props.h - y - 1;
                    out_y = x;
                    break;
                case ROTATE_180:
                    out_x = source.props.w - x - 1;
                    out_y = source.props.h - y - 1;
                    break;
                case ROTATE_270:
                    out_x = y;
                    out_y = source.props.w - x - 1;
                    break;
                case ROTATE_0:
                default:
                    out_x = x;
                    out_y = y;
                    break;
            }

            RGBColor *s_pixel = &source.data[(y*source.props.w + x)];
            RGBColor *d_pixel = &output.data[(out_y*output.props.w + out_x)];

            MemoryCopy(d_pixel, s_pixel, sizeof(RGBColor));
        }
    }

    s32 adj_degrees = direction == CW ? degrees : -degrees;
    s32 output_rotation = source.props.rotation + adj_degrees;
    if(output_rotation < 0) output_rotation += 360;

    stopwatch_end(source.stopwatch, S(__func__));

    return output;
}

// preserves aspect ratio
// bilinear scaling for now

Image image_scale(Image source, u32 target_width, u32 target_height, b32 remove_margins)
{
    stopwatch_begin(source.stopwatch, S(__func__));

    Image output = source;

    b32 landscape = (source.props.w >= source.props.h);

    if(landscape)
    {
        output.props.scale = (f32)target_width / source.props.w;
        output.props.w = target_width;
        output.props.h = ceil(source.props.h * output.props.scale);
        if(remove_margins) target_height = output.props.h;
        output.props.pad_y = ABS(target_height - output.props.h) / 2;
    }
    else
    {
        output.props.scale = (f32)target_height / source.props.h;
        output.props.h = target_height;
        output.props.w = ceil(source.props.w * output.props.scale);
        if(remove_margins) target_width = output.props.w;
        output.props.pad_x = ABS(target_width - output.props.w) / 2;
    }

    output.data = PUSH_ARRAY(source.arena, RGBColor, target_width * target_height);

    // resize

    f32 ratio_x = (f32)(source.props.w - 1) / (output.props.w - 1);
    f32 ratio_y = (f32)(source.props.h - 1) / (output.props.h - 1);

    for(u32 j = 0; j < output.props.h; ++j)
    {
        for(u32 i = 0; i < output.props.w; ++i)
        {
            f32 src_x = i * ratio_x;
            f32 src_y = j * ratio_y;

            u32 x_l = floor(src_x);
            u32 y_l = floor(src_y);
            u32 x_h = ceil(src_x);
            u32 y_h = ceil(src_y);

            RGBColor p11 = source.data[(source.props.w)*y_l + x_l];
            RGBColor p12 = source.data[(source.props.w)*y_h + x_l];
            RGBColor p21 = source.data[(source.props.w)*y_l + x_h];
            RGBColor p22 = source.data[(source.props.w)*y_h + x_h];

            f32 weight_x = src_x - x_l;
            f32 weight_y = src_y - y_l;

            RGBColor r1 = {
                (p21.r * weight_x) + (p11.r * (1.0 - weight_x)),
                (p21.g * weight_x) + (p11.g * (1.0 - weight_x)),
                (p21.b * weight_x) + (p11.b * (1.0 - weight_x))
            };

            RGBColor r2 = {
                (p22.r * weight_x) + (p12.r * (1.0 - weight_x)),
                (p22.g * weight_x) + (p12.g * (1.0 - weight_x)),
                (p22.b * weight_x) + (p12.b * (1.0 - weight_x))
            };

            RGBColor p = {
                (r2.r * weight_y) + (r1.r * (1.0 - weight_y)),
                (r2.g * weight_y) + (r1.g * (1.0 - weight_y)),
                (r2.b * weight_y) + (r1.b * (1.0 - weight_y))
            };

            u32 dst_i = i + output.props.pad_x;
            u32 dst_j = j + output.props.pad_y;
            MemoryCopy(&output.data[dst_j*target_width + dst_i], &p, sizeof(RGBColor));
        }
    }

    output.props.w = landscape ? output.props.w : target_width;
    output.props.h = landscape ? target_height  : output.props.h;

    stopwatch_end(source.stopwatch, S(__func__));

    return output;
}

static f32 lanczos_kernel(f32 x, u32 a)
{
    if(x == 0.0f)                 return 1.0f;
    if(x < -(f32)a || x > (f32)a) return 0.0f;
    f32 pi_x = PI * x;
    return ((f32)a * sinf(pi_x) * sinf(pi_x / (f32)a)) / (pi_x * pi_x);
}

static LanczosFilter *lanczos_precompute(Arena *arena, u32 src_len, u32 dst_len, u32 a)
{
    f32 ratio         = (f32)src_len / (f32)dst_len;
    f32 filter_scale  = MAX(1.0f, ratio);
    f32 filter_radius = (f32)a * filter_scale;
    s32 tap_count     = (s32)ceilf(filter_radius * 2.0f);

    LanczosFilter *filters = PUSH_ARRAY(arena, LanczosFilter, dst_len);
    for(u32 i = 0; i < dst_len; ++i)
    {
        f32 src_center     = (i + 0.5f) * ratio - 0.5f;
        s32 start          = (s32)floorf(src_center - filter_radius);
        filters[i].start   = start;
        filters[i].count   = tap_count;
        filters[i].weights = PUSH_ARRAY(arena, f32, tap_count);

        f32 weight_sum = 0.0f;
        for(s32 t = 0; t < tap_count; ++t)
        {
            f32 dist               = (src_center - (start + t)) / filter_scale;
            f32 w                  = lanczos_kernel(dist, a);
            filters[i].weights[t]  = w;
            weight_sum            += w;
        }
        if(weight_sum == 0.0f) weight_sum = 1.0f;
        f32 inv_sum = 1.0f / weight_sum;
        for(s32 t = 0; t < tap_count; ++t)
            filters[i].weights[t] *= inv_sum;
    }
    return filters;
}

Image image_scale_lanczos(Image source, u32 target_width, u32 target_height, u32 a, b32 remove_margins)
{
    stopwatch_begin(source.stopwatch, S(__func__));

    Image output  = source;
    b32 landscape = (source.props.w >= source.props.h);
    if(landscape)
    {
        output.props.scale = (f32)target_width / source.props.w;
        output.props.w     = target_width;
        output.props.h     = (u32)ceilf(source.props.h * output.props.scale);
        if(remove_margins) target_height = output.props.h;
        output.props.pad_y = ABS(target_height - output.props.h) / 2;
    }
    else
    {
        output.props.scale = (f32)target_height / source.props.h;
        output.props.h     = target_height;
        output.props.w     = (u32)ceilf(source.props.w * output.props.scale);
        if(remove_margins) target_width = output.props.w;
        output.props.pad_x = ABS(target_width - output.props.w) / 2;
    }

    u32 src_w = source.props.w;
    u32 src_h = source.props.h;
    u32 out_w = output.props.w;
    u32 out_h = output.props.h;

    LanczosFilter *filters_x = lanczos_precompute(source.arena, src_w, out_w, a);
    LanczosFilter *filters_y = lanczos_precompute(source.arena, src_h, out_h, a);

    // Horizontal pass: source -> intermediate (out_w x src_h)
    f32 *intermediate = PUSH_ARRAY(source.arena, f32, out_w * src_h * 3);
    for(u32 j = 0; j < src_h; ++j)
    {
        for(u32 i = 0; i < out_w; ++i)
        {
            LanczosFilter fx = filters_x[i];
            f32 acc_r = 0.0f, acc_g = 0.0f, acc_b = 0.0f;
            for(s32 t = 0; t < fx.count; ++t)
            {
                s32 sx     = fx.start + t;
                s32 csx    = sx < 0 ? 0 : (sx >= (s32)src_w ? (s32)src_w - 1 : sx);
                RGBColor p = source.data[j * src_w + csx];
                f32 w      = fx.weights[t];
                acc_r     += p.r * w;
                acc_g     += p.g * w;
                acc_b     += p.b * w;
            }
            u32 idx             = (j * out_w + i) * 3;
            intermediate[idx+0] = acc_r;
            intermediate[idx+1] = acc_g;
            intermediate[idx+2] = acc_b;
        }
    }

    // Vertical pass: intermediate -> output (out_w x out_h)
    output.data = PUSH_ARRAY(source.arena, RGBColor, target_width * target_height);
    for(u32 j = 0; j < out_h; ++j)
    {
        LanczosFilter fy = filters_y[j];
        for(u32 i = 0; i < out_w; ++i)
        {
            f32 acc_r = 0.0f, acc_g = 0.0f, acc_b = 0.0f;
            for(s32 t = 0; t < fy.count; ++t)
            {
                s32 sy  = fy.start + t;
                s32 csy = sy < 0 ? 0 : (sy >= (s32)src_h ? (s32)src_h - 1 : sy);
                u32 idx = (csy * out_w + i) * 3;
                f32 w   = fy.weights[t];
                acc_r  += intermediate[idx+0] * w;
                acc_g  += intermediate[idx+1] * w;
                acc_b  += intermediate[idx+2] * w;
            }
            u32 dst_i  = i + output.props.pad_x;
            u32 dst_j  = j + output.props.pad_y;
            RGBColor p = {
                (u8)(acc_r < 0.0f ? 0 : acc_r > 255.0f ? 255 : (u8)acc_r),
                (u8)(acc_g < 0.0f ? 0 : acc_g > 255.0f ? 255 : (u8)acc_g),
                (u8)(acc_b < 0.0f ? 0 : acc_b > 255.0f ? 255 : (u8)acc_b),
            };
            MemoryCopy(&output.data[dst_j * target_width + dst_i], &p, sizeof(RGBColor));
        }
    }

    output.props.w = landscape ? out_w : target_width;
    output.props.h = landscape ? target_height : out_h;

    stopwatch_end(source.stopwatch, S(__func__));

    return output;
}

Rotation image_get_rotation_from_file(char *file_path)
{
    Rotation rotation = ROTATE_0;

    // get orientation using libexif
    ExifData *ed = exif_data_new_from_file(file_path);

    if(ed)
    {
        ExifEntry *entry = exif_data_get_entry(ed, EXIF_TAG_ORIENTATION);

        if(entry)
        {
            ExifShort orient = exif_get_short(entry->data, exif_data_get_byte_order(ed));

            switch(orient)
            {
                case 1: case 2: rotation = ROTATE_0;   break;
                case 3: case 4: rotation = ROTATE_180; break;
                case 6: case 7: rotation = ROTATE_270; break;
                case 5: case 8: rotation = ROTATE_90;  break;
                default: rotation = ROTATE_0;  break; 
            }
        }

        exif_data_unref(ed);
    }

    return rotation;
}

void image_print(Image *image, LogLevel ll)
{
    os_log(ll, __FILE__, __LINE__, "===================");
    os_log(ll, __FILE__, __LINE__, "Image %p:", image);
    os_log(ll, __FILE__, __LINE__, "        w: %u", image->props.w);
    os_log(ll, __FILE__, __LINE__, "        h: %u", image->props.h);
    os_log(ll, __FILE__, __LINE__, "      rot: %u", image->props.rotation);
    os_log(ll, __FILE__, __LINE__, "    arena: %p", image->arena);
    os_log(ll, __FILE__, __LINE__, "===================");
}
