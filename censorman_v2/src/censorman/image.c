
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

Image image_nil()
{
    Image image = {0};
    return image;
}

Image image_load(Arena *arena, String path)
{
    Image image = {0};
    image.arena = arena;

    ArenaTemp scratch = scratch_begin();
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
        return image;
    }

    image.w = w;
    image.h = h;
    image.n = 3;
    image.data = PUSH_ARRAY(image.arena, RGBColor, w*h);
    image.scale = 1.0;

    // pack RGB (remove alpha channel if needed)
    for(s32 i = 0; i < w*h; ++i)
    {
        RGBColor *pixel = &image.data[i];

        pixel->r = data[i*n+0];
        pixel->g = data[i*n+1];
        pixel->b = data[i*n+2];
    }

    logv("Loaded image " STR_FMT " [w: %u h: %u n: %u]", STR_ARG(path), image.w,image.h,image.n);

    // free buffer
    stbi_image_free(data);

    return image;
}

b32 image_save(Image *image, String path)
{
    ArenaTemp scratch = scratch_begin();

    char *output_file_cstr = string_to_cstr(scratch.arena, path);

    s32 res = stbi_write_png(output_file_cstr, image->w, image->h, image->n, image->data, image_step(image));

    scratch_end(scratch);

    if(res == 0)
    {
        loge("Failed to write output");
        return false;
    }
    return true;
}

inline u32 image_step(Image *image)
{
    return image->w * image->n;
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

    Image output = {0};
    
    b32 dim_flipped = (degrees == ROTATE_90 || degrees == ROTATE_270);

    output.data          = PUSH_ARRAY(source.arena, RGBColor, source.w * source.h);
    output.w             = dim_flipped ? source.h : source.w;
    output.h             = dim_flipped ? source.w : source.h;
    output.n             = source.n;
    output.rotation      = source.rotation;
    output.arena         = source.arena;

    s32 out_x = 0;
    s32 out_y = 0;

    if(direction == CCW)
    {
        if(degrees == ROTATE_90)       degrees = ROTATE_270;
        else if(degrees == ROTATE_270) degrees = ROTATE_90;
    }

    for(int y = 0; y < source.h; ++y)
    {
        for(int x = 0; x < source.w; ++x)
        {
            switch(degrees)
            {
                case ROTATE_90:
                    out_x = source.h - y - 1;
                    out_y = x;
                    break;
                case ROTATE_180:
                    out_x = source.w - x - 1;
                    out_y = source.h - y - 1;
                    break;
                case ROTATE_270:
                    out_x = y;
                    out_y = source.w - x - 1;
                    break;
                case ROTATE_0:
                default:
                    out_x = x;
                    out_y = y;
                    break;
            }

            RGBColor *s_pixel = &source.data[(y*source.w + x)];
            RGBColor *d_pixel = &output.data[(out_y*output.w + out_x)];

            MemoryCopy(d_pixel, s_pixel, sizeof(RGBColor));
        }
    }

    return output;
}

// preserves aspect ratio
// bilinear scaling for now

Image image_scale(Image source, u32 target_width, u32 target_height)
{
    Image image_scaled = {0};

    image_scaled.n = source.n;
    image_scaled.arena = source.arena;
    image_scaled.rotation = source.rotation;

    b32 landscape = (source.w >= source.h);

    if(landscape)
    {
        image_scaled.scale = (f32)target_width / source.w;
        image_scaled.w = target_width;
        image_scaled.h = source.h * image_scaled.scale;
        image_scaled.pad_y = ABS(target_height - image_scaled.h) / 2;
    }
    else
    {
        image_scaled.scale = (f32)target_height / source.h;
        image_scaled.h = target_height;
        image_scaled.w = source.w * image_scaled.scale;
        image_scaled.pad_x = ABS(target_width - image_scaled.w) / 2;
    }

    image_scaled.data = PUSH_ARRAY(source.arena, RGBColor, target_width * target_height);

    // resize

    f32 ratio_x = (f32)(source.w - 1) / (image_scaled.w - 1);
    f32 ratio_y = (f32)(source.h - 1) / (image_scaled.h - 1);

    for(u32 j = 0; j < image_scaled.h; ++j)
    {
        for(u32 i = 0; i < image_scaled.w; ++i)
        {
            f32 src_x = i * ratio_x;
            f32 src_y = j * ratio_y;

            u32 x_l = floor(src_x);
            u32 y_l = floor(src_y);
            u32 x_h = ceil(src_x);
            u32 y_h = ceil(src_y);

            RGBColor p11 = source.data[(source.w)*y_l + x_l];
            RGBColor p12 = source.data[(source.w)*y_h + x_l];
            RGBColor p21 = source.data[(source.w)*y_l + x_h];
            RGBColor p22 = source.data[(source.w)*y_h + x_h];

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

            u32 dst_i = i + image_scaled.pad_x;
            u32 dst_j = j + image_scaled.pad_y;
            MemoryCopy(&image_scaled.data[dst_j*target_width + dst_i], &p, sizeof(RGBColor));
        }
    }

    image_scaled.w = landscape ? image_scaled.w : target_width;
    image_scaled.h = landscape ? target_height  : image_scaled.h;

    return image_scaled;
    
}

void image_print(Image *image)
{
    logi("===================");
    logi("Image %p:", image);
    logi("    w: %u", image->w);
    logi("    h: %u", image->h);
    logi("    n: %u", image->n);
    logi("  rot: %u", image->rotation);
    logi("arena: %p", image->arena);
    logi("===================");
}
