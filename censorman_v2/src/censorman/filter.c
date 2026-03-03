
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

void filter_blackout(Image *image, Box *box)
{
    _draw_box(image, box, (RGBColor){0,0,0}, true, 1.0);
}

void filter_outline(Image *image, Box *box)
{
    _draw_box(image, box, (RGBColor){0,0,0}, false, 1.0);
}

RGBColor blend_color(RGBColor *pixel, RGBColor color, f32 opacity)
{
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
        RGBColor r = opacity == 1.0 ? color : blend_color(curr+i, color, opacity);
        MemoryCopy(curr+i, &r, sizeof(r));
    }

    curr += image->w;

    if(filled)
    {
        for(s32 j = 0; j < box->h-1; ++j)
        {
            for(s32 i = 0; i < box->w; ++i)
            {
                RGBColor r = opacity == 1.0 ? color : blend_color(curr+i,color,opacity);
                MemoryCopy(curr+i, &r, sizeof(r));
            }
            curr += image->w;
        }
    }
    else
    {
        for(s32 i = 0; i < box->h-1; ++i)
        {
            RGBColor cl = opacity == 1.0 ? color : blend_color(curr,color,opacity);
            RGBColor cr = opacity == 1.0 ? color : blend_color(curr+box->w,color,opacity);

            MemoryCopy(curr, &cl, sizeof(cl));          // left pixel
            MemoryCopy(curr + box->w, &cr, sizeof(cr)); // right pixel

            curr += image->w;
        }
    }

    for(s32 i = 0; i <= box->w; ++i)
    {
        RGBColor r = opacity == 1.0 ? color : blend_color(curr+i,color,opacity);
        MemoryCopy(curr+i, &r, sizeof(r));
    }
}
