
typedef enum
{
    FILTER_TYPE_NONE = 0,
    FILTER_TYPE_BLACKOUT,
    FILTER_TYPE_BLUR_BOX,
    FILTER_TYPE_BLUR_GAUSSIAN,
    FILTER_TYPE_PIXELATE,
    FILTER_TYPE_TEXTURE,
    FILTER_TYPE_MAX,
} FilterType;

typedef struct
{
    FilterType type;

    f32 block_scale;   // (Pixelate) [0.0 - 1.0]
    f32 blur_strength; // (Blur) [0.0 - 1.0]

    String texture_path;

} Filter;

String     filter_to_string(FilterType type);
FilterType filter_from_string(String str);

void filter_apply(Filter filter, Image *image, Box *box);

// filters
void filter_blackout(Image *image, Box *box);
void filter_blur_gaussian(Image *image, Box *box, f32 blur_strength);
void filter_pixelate(Image* image, Box *box, f32 block_scale);

void filter_draw_debug_info(Image *image, Box *box);

