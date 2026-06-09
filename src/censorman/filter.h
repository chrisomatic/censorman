
typedef enum
{
    FILTER_TYPE_NONE = 0,
    FILTER_TYPE_BLACKOUT,
    FILTER_TYPE_BLUR_BOX,
    FILTER_TYPE_BLUR_GAUSSIAN,
    FILTER_TYPE_PIXELATE,
    FILTER_TYPE_SCRAMBLE,
    FILTER_TYPE_TEXTURE,
    FILTER_TYPE_MAX,
} FilterType;

typedef enum
{
    FACIAL_FEATURE_NONE     = 0,
    FACIAL_FEATURE_EYES     = (1<<0),
    FACIAL_FEATURE_NOSE     = (1<<1),
    FACIAL_FEATURE_MOUTH    = (1<<2),
    FACIAL_FEATURE_CHEEKS   = (1<<3),
    FACIAL_FEATURE_FOREHEAD = (1<<4),
} FacialFeature;

typedef struct
{
    FilterType type;
    f32 param; // typically in range [0.0, 1.0]
    String texture_path;
    b32 elliptical;
} Filter;

String     filter_to_string(FilterType type);
FilterType filter_from_string(String str);

void filter_apply(Filter filter, Image *image, Box *box);

// filters
void filter_blackout(Image *image, Box *box);
void filter_blur_gaussian(Image *image, Box *box, f32 blur_strength);
void filter_blur_box(Image *image, Box *box, f32 blur_strength);
void filter_pixelate(Image* image, Box *box, f32 block_scale);
void filter_scramble(Image *image, Box *box);
void filter_texture(Image *image, Box *box);

void filter_draw_debug_info(Image *image, BoxFrame *box_frame, f32 box_padding, b32 no_labels);
