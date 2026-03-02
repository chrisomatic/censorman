
void *detect(void *args)
{
    DetectArgs *detect_args = (DetectArgs *)args;

    DetectType type = detect_args->type;
    Image *image    = detect_args->image;
    List *boxes     = detect_args->boxes;

    return NULL;
}
