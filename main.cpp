#include <stdio.h>
#include "base.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "detect.h"

// detect.h
//    int detect_faces(Image*, Rect* rects)
// transform.h
//    void transform_black(Image*, Rect)
// net.h

void reverse_rgb_order(Image *image)
{
    printf("Reversing RGB Order... %d\n", image->w*image->h);
    for(int i = 0; i < image->w*image->h; ++i)
    {
        int n = i*image->channels;
        unsigned char temp = image->data[n+0];
        image->data[n+0] = image->data[i*3+2]; // R -> B
        image->data[n+2] = temp;               // B -> R
    }
}

int main(int argc, char** args)
{
    Image image = {0};

    image.data = stbi_load("images/test1.jpg", &image.w, &image.h, &image.channels, 0);

    if(!image.data)
    {
        fprintf(stderr, "Failed to load image\n");
        return 1;
    }

    printf("Loaded image! w: %d h: %d n: %d\n", image.w,image.h,image.channels);

    if(image.channels < 3)
    {
        fprintf(stderr, "Not enough channels on image.\n");
        return 1;
    }


    reverse_rgb_order(&image);


    printf("Detecting faces...\n");
    Rect* rects = (Rect*)malloc(100*sizeof(Rect));
    int num_faces = detect_faces(&image, rects);
    printf("%d faces detected\n", num_faces);
    int step = image.w*image.channels;

    for(int i = 0; i < num_faces; ++i)
    {
        Rect* r = &rects[i];
        if(r->confidence < 80) continue;

        // black out pixels (for now)
        for(int j = r->y; j < r->y + r->h; ++j)
        {
            for(int k = r->x; k < r->x + r->w; ++k)
            {
                int kn = k*image.channels;
                image.data[j*step+kn+0] = 0x00;
                image.data[j*step+kn+1] = 0x00;
                image.data[j*step+kn+2] = 0x00;
            }
        }
    }

    reverse_rgb_order(&image);

    printf("Writing output file...\n");
    int res = stbi_write_png("output/out.png", image.w, image.h, image.channels, image.data, step);
    if(res == 0)
    {
        fprintf(stderr, "Failed to write output!\n");
        return 1;
    }

    return 0;
}
