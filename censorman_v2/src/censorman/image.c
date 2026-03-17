
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Image image_nil()
{
    Image image = {0};
    return image;
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

    s32 res = stbi_write_png(output_file_cstr, image->props.w, image->props.h, 3, image->data, image_step(image));

    scratch_end(scratch);

    stopwatch_end(image->stopwatch, S(__func__));

    if(res == 0)
    {
        loge("Failed to write output");
        return false;
    }

    return true;
}

inline u32 image_step(Image *image)
{
    return image->props.w * 3;
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

Image image_scale(Image source, u32 target_width, u32 target_height)
{
    stopwatch_begin(source.stopwatch, S(__func__));

    Image output = source;

    b32 landscape = (source.props.w >= source.props.h);

    if(landscape)
    {
        output.props.scale = (f32)target_width / source.props.w;
        output.props.w = target_width;
        output.props.h = source.props.h * output.props.scale;
        output.props.pad_y = ABS(target_height - output.props.h) / 2;
    }
    else
    {
        output.props.scale = (f32)target_height / source.props.h;
        output.props.h = target_height;
        output.props.w = source.props.w * output.props.scale;
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

void image_print(Image *image)
{
    logi("===================");
    logi("Image %p:", image);
    logi("        w: %u", image->props.w);
    logi("        h: %u", image->props.h);
    logi("      rot: %u", image->props.rotation);
    logi("    arena: %p", image->arena);
    logi("===================");
}
