
void filter_apply(Filter filter, Image *image, Box *box)
{
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
    return;
}

static void _draw_box(Image *image, Box *box, RGBColor color, b32 filled, f32 opacity);
static void _convolve(Image *src, Image *dst, Box *roi, f32 *kernel, s32 k_size, bool horizontal);

void filter_blackout(Image *image, Box *box)
{
    _draw_box(image, box, (RGBColor){0,0,0}, true, 1.0);
}

void filter_outline(Image *image, Box *box)
{
    _draw_box(image, box, (RGBColor){0,0,0}, false, 1.0);
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
    ArenaTemp scratch = scratch_begin();

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
    _convolve(image, &tmp, box, kernel, k_size, true);

    // vertical pass (write back into original image buffer)
    _convolve(&tmp, image, box, kernel, k_size, false);

    scratch_end(scratch);
}

static void _convolve(Image *src, Image *dst, Box *roi, f32 *kernel, s32 k_size, bool horizontal)
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

RGBColor blend_color(RGBColor *pixel, RGBColor color, f32 opacity)
{
    if(opacity == 1.0)
        return color;

    RGBColor ret_color = {0};

    ret_color.r = opacity*color.r + (1.0 - opacity)*pixel->r;
    ret_color.g = opacity*color.g + (1.0 - opacity)*pixel->g;
    ret_color.b = opacity*color.b + (1.0 - opacity)*pixel->b;

    return ret_color;
}

static void _draw_box(Image* image, Box *box, RGBColor color, b32 filled, f32 opacity)
{
    RGBColor *start = &image->data[box->y*image->w + box->x];
    RGBColor *curr = start;

    // draw first line
    for(s32 i = 0; i <= box->w; ++i)
    {
        RGBColor *r = &curr[i];
        *r = blend_color(r, color, opacity);
    }

    curr += image->w;

    if(filled)
    {
        for(s32 j = 0; j < box->h-1; ++j)
        {
            for(s32 i = 0; i < box->w; ++i)
            {
                RGBColor *r = &curr[i];
                *r = blend_color(r, color, opacity);
            }
            curr += image->w;
        }
    }
    else
    {
        for(s32 i = 0; i < box->h-1; ++i)
        {
            RGBColor *cl = &curr[0];
            RGBColor *cr = &curr[box->w];

            *cl = blend_color(cl, color, opacity);
            *cr = blend_color(cr, color, opacity);

            curr += image->w;
        }
    }

    for(s32 i = 0; i <= box->w; ++i)
    {
        RGBColor *r = &curr[i];
        *r = blend_color(r, color, opacity);
    }
}
