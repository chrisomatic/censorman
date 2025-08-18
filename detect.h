#include "base.h"
#include "facedetectcnn.h"

void* detect_faces(void* arg)
{
    Image* image = (Image*)arg;

    int *results = facedetect_cnn(image->detect_buffer,image->data,image->w,image->h,image->step); 

    int num_faces = (results ? *results : 0);

    void* ret = arena_alloc((Arena*)image->arena, sizeof(int) + num_faces*sizeof(Rect));

    int offset = 0;
    
    memcpy(ret, &num_faces, sizeof(int));
    offset += sizeof(int);

    for(int i = 0; i < num_faces; ++i)
    {
        short *p = ((short*)(results+1)) + 16*i;

        Rect *r = (Rect*)((u8*)(ret)+offset);

        r->confidence = p[0];
        r->x = p[1] + (image->subx*image->w);
        r->y = p[2] + (image->suby*image->h);
        r->w = p[3];
        r->h = p[4];

        offset += sizeof(Rect);
    }

    free(results);

    pthread_exit(ret);
}
