

typedef struct
{
    fz_device base;
    List *images;
    Arena *arena;
} image_capture_device;

static void callback_image_filler(fz_context *ctx, fz_device *dev_,
                                   fz_image *image, fz_matrix ctm,
                                   float alpha, fz_color_params color_params)
{
    image_capture_device *dev = (image_capture_device *)dev_;

    // Decode to RGB24 directly — no alpha, no padding
    fz_colorspace *rgb = fz_device_rgb(ctx);
    fz_pixmap *pm = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
    fz_pixmap *rgb_pm = fz_convert_pixmap(ctx, pm, rgb, NULL, NULL,
                                           fz_default_color_params, 0);
    fz_drop_pixmap(ctx, pm);

    u32 w = rgb_pm->w;
    u32 h = rgb_pm->h;
    u32 n = rgb_pm->n; // should be 3 for RGB

    RGBColor *data = PUSH_ARRAY(dev->arena, RGBColor, w * h);

    // MuPDF RGB pixmap stride may be padded, copy row by row
    for (u32 y = 0; y < h; y++)
    {
        u8 *src = rgb_pm->samples + y * rgb_pm->stride;
        RGBColor *dst = data + y * w;
        for (u32 x = 0; x < w; x++)
        {
            dst[x].r = src[x * n + 0];
            dst[x].g = src[x * n + 1];
            dst[x].b = src[x * n + 2];
        }
    }

    fz_drop_pixmap(ctx, rgb_pm);

    Image img = {0};
    img.data       = data;
    img.arena      = dev->arena;
    img.props.w    = w;
    img.props.h    = h;
    img.props.scale = 1.0f;
    img.props_orig  = img.props;

    list_add(dev->images, &img);
}

PDF pdf_open(Arena *arena, String file_path)
{
    PDF pdf = {0};

    pdf.arena = arena;
    pdf.ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    fz_register_document_handlers(pdf.ctx);

    char* file_path_cstr = string_to_cstr(pdf.arena, file_path);
    pdf.doc = (fz_document *)pdf_open_document(pdf.ctx, file_path_cstr);

    pdf.page_count = pdf_count_pages(pdf.ctx, (pdf_document *)pdf.doc);
    if (pdf.page_count > 0)
    {
        fz_page *page = fz_load_page(pdf.ctx, pdf.doc, 0);

        List images = list_create(arena, sizeof(Image));

        fz_context *ctx = pdf.ctx;
        image_capture_device *dev = fz_new_derived_device(ctx, image_capture_device);
        dev->base.fill_image = callback_image_filler;
        dev->images = &images;
        dev->arena  = arena;

        fz_run_page(pdf.ctx, page, (fz_device *)dev, fz_identity, NULL);
        fz_drop_device(pdf.ctx, (fz_device *)dev);
        fz_drop_page(pdf.ctx, page);

        // images is now populated
        for(s32 i = 0; i < images.count; ++i)
        {
            Image *img = (Image *)list_get(&images, i);
            logi("Image: %dx%d", img->props.w, img->props.h);
        }
    }

    logi("Opened '%s': %d page(s)", file_path_cstr, pdf.page_count);

    return pdf;
}

b32 pdf_is_valid(PDF *pdf)
{
    return (pdf->ctx != NULL && pdf->doc != NULL);
}

void pdf_save(PDF *pdf, String pdf_path)
{
    if(!pdf_is_valid(pdf))
    {
        loge("pdf_save: invalid PDF");
        return;
    }

    char *pdf_path_cstr = string_to_cstr(pdf->arena, pdf_path);

    fz_try(pdf->ctx)
    {
        pdf_write_options opts = pdf_default_write_options;
        opts.do_compress        = 1;
        opts.do_compress_images = 1;
        opts.do_garbage         = 2;  // full GC pass
        opts.do_clean           = 1;

        pdf_save_document(pdf->ctx, (pdf_document *)pdf->doc, pdf_path_cstr, &opts);
        logi("pdf_save: saved to '%s'", pdf_path_cstr);
    }
    fz_catch(pdf->ctx)
    {
        loge("pdf_save: %s", fz_caught_message(pdf->ctx));
    }
}

void pdf_close(PDF *pdf)
{
    fz_drop_document(pdf->ctx, (fz_document *)pdf->doc);
    fz_drop_context(pdf->ctx);
}

