
typedef enum
{
    FILTER_TYPE_NONE = 0,
    FILTER_TYPE_BLACKOUT,
    FILTER_TYPE_BLUR_BOX,
    FILTER_TYPE_BLUR_GAUSSIAN,
    FILTER_TYPE_PIXELATE,
    FILTER_TYPE_TEXTURE,
} FilterType;

typedef struct
{
    FilterType type;

} Filter;

void filter_apply(Filter filter, Image *image, Box box);


