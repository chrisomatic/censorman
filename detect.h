#include "base.h"
#include "facedetectcnn.h"

U8 detect_buffer[0x9000] = {0};

void* detect_faces(DetectInfo* di)
{
    U32 *results = facedetect_cnn(detect_buffer,di->image->data,di->image->w,di->image->h,di->image->w*di->image->channels); 

    U32 num_faces = (results ? *results : 0);

    Rect* rects = (Rect*)malloc(num_faces*sizeof(Rect));

    for(U32 i = 0; i < num_faces; ++i)
    {
        short *p = ((short*)(results+1)) + 16*i;

        Rect *r = &rects[i];

        r->confidence = p[0];
        r->x = p[1];
        r->y = p[2];
        r->w = p[3];
        r->h = p[4];
    }

    pthread_exit(rects);
}
