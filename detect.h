#include "base.h"
#include "facedetectcnn.h"

u8 detect_buffer[0x9000] = {0};

void* detect_faces(void* _image)
{
    Image* image = (Image*)_image;
    int *results = facedetect_cnn(detect_buffer,image->data,image->w,image->h,image->w*image->channels); 

    int num_faces = (results ? *results : 0);

    printf("Found %d faces!\n", num_faces);

    u8* ret = (u8*)malloc(sizeof(int) + num_faces*sizeof(Rect));

    int offset = 0;
    memcpy(ret, &num_faces, sizeof(int));
    offset += sizeof(int);

    for(int i = 0; i < num_faces; ++i)
    {
        short *p = ((short*)(results+1)) + 16*i;

        Rect *r = (Rect*)(ret+offset);

        r->confidence = p[0];
        r->x = p[1];
        r->y = p[2];
        r->w = p[3];
        r->h = p[4];

        offset += sizeof(Rect);
    }

    pthread_exit(ret);
    return NULL;
}
