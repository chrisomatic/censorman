#include "base.h"

void transform_draw_rect(Image* image, Rect r, Color c, bool filled)
{
    int step = image->w*image->channels;

    // draw first line
    for(int k = r.x; k < r.x + r.w; ++k)
    {
        image->data[k*image->channels+0] = c.r;
        image->data[k*image->channels+1] = c.g;
        image->data[k*image->channels+2] = c.b;
    }

    for(int j = r.y; j < r.y + r.h; ++j)
    {
        if(filled)
        {
            for(int k = r.x; k < r.x + r.w; ++k)
            {
                int kn = k*image->channels;

                image->data[j*step+kn+0] = 0x00;
                image->data[j*step+kn+1] = 0x00;
                image->data[j*step+kn+2] = 0x00;
            }
        }
        else
        {
            // left pixel
            int idx = r.x*image->channels + j*step;
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
        image->data[(r.y+r.h)*step + k*image->channels+0] = c.r;
        image->data[(r.y+r.h)*step + k*image->channels+1] = c.g;
        image->data[(r.y+r.h)*step + k*image->channels+2] = c.b;
    }
}

void transform_black_out(Image* image, Rect r)
{
    int step = image->w*image->channels;

    for(int j = r.y; j < r.y + r.h; ++j)
    {
        for(int k = r.x; k < r.x + r.w; ++k)
        {
            int kn = k*image->channels;

            image->data[j*step+kn+0] = 0x00;
            image->data[j*step+kn+1] = 0x00;
            image->data[j*step+kn+2] = 0x00;
        }
    }
}
