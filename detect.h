#include "base.h"
#include "facedetectcnn.h"

unsigned char *detect_buffer = NULL;

int detect_faces(Image* image, Rect* rects)
{
    if(!detect_buffer)
        detect_buffer = (unsigned char*)malloc(0x9000);

    if(!detect_buffer)
    {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 1;
    }

    int *results = facedetect_cnn(detect_buffer,image->data,image->w,image->h,image->w*image->channels); 

    int num_faces = (results ? *results : 0);

    for(int i = 0; i < num_faces; ++i)
    {
        short *p = ((short*)(results+1)) + 16*i;

        Rect *r = &rects[i];

        r->confidence = p[0];
        r->x = p[1];
        r->y = p[2];
        r->w = p[3];
        r->h = p[4];
    }

    return num_faces;
}
