
typedef struct
{
    fz_device base;
    List *images;
    Arena *arena;
} image_capture_device;

static void callback_image_filler(fz_context *ctx, fz_device *dev_,
                                  fz_image *image, fz_matrix ctm,
                                  f32 alpha, fz_color_params color_params)
{
    image_capture_device *dev = (image_capture_device *)dev_;

    // decode to RGB24
    fz_colorspace *rgb = fz_device_rgb(ctx);
    fz_pixmap *pm = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
    fz_pixmap *rgb_pm = fz_convert_pixmap(ctx, pm, rgb, NULL, NULL, fz_default_color_params, 0);
    fz_drop_pixmap(ctx, pm);

    u32 w = rgb_pm->w;
    u32 h = rgb_pm->h;
    u32 n = rgb_pm->n; // should be 3 for RGB

    RGBColor *data = PUSH_ARRAY(dev->arena, RGBColor, w * h);

    // copy row by row since there may be padding
    for(u32 y = 0; y < h; ++y)
    {
        u8 *src = rgb_pm->samples + y * rgb_pm->stride;
        RGBColor *dst = data + y * w;
        for(u32 x = 0; x < w; ++x)
        {
            dst[x].r = src[x * n + 0];
            dst[x].g = src[x * n + 1];
            dst[x].b = src[x * n + 2];
        }
    }

    fz_drop_pixmap(ctx, rgb_pm);

    PDFImage pdf_img = {0};

    pdf_img.total_changes     = 0;
    pdf_img.image.data        = data;
    pdf_img.image.arena       = dev->arena;
    pdf_img.image.props.w     = w;
    pdf_img.image.props.h     = h;
    pdf_img.image.props.scale = 1.0f;
    pdf_img.image.props_orig  = pdf_img.image.props;
    
    logv("Found PDF image (w: %d, h: %d)", w, h);

    list_add(dev->images, &pdf_img);
}

PDF pdf_nil()
{
    PDF pdf = {0};
    return pdf;
}

PDF pdf_open(Arena *arena, String file_path)
{
    PDF pdf = {0};

    pdf.arena = arena;
    pdf.ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    fz_try(pdf.ctx)
    {
        fz_register_document_handlers(pdf.ctx); 
    }
    fz_catch(pdf.ctx)
    {
        fz_report_error(pdf.ctx);
        fz_drop_context(pdf.ctx);
        return pdf_nil();
    }

    char* file_path_cstr = string_to_cstr(pdf.arena, file_path);

    fz_try(pdf.ctx)
    {
        pdf.doc = (fz_document *)pdf_open_document(pdf.ctx, file_path_cstr);
    }
    fz_catch(pdf.ctx)
    {
        fz_report_error(pdf.ctx);
        fz_drop_context(pdf.ctx);
        return pdf_nil();
    }

    fz_try(pdf.ctx)
    {
        pdf.page_count = pdf_count_pages(pdf.ctx, (pdf_document *)pdf.doc);
    }
    fz_catch(pdf.ctx)
    {
        fz_report_error(pdf.ctx);
        fz_drop_context(pdf.ctx);
        return pdf_nil();
    }

    logi("Opened '%s': %d page(s)", file_path_cstr, pdf.page_count);

    return pdf;
}

b32 pdf_is_valid(PDF *pdf)
{
    return (pdf->ctx != NULL && pdf->doc != NULL);
}

PDFPage pdf_open_page(PDF *pdf, s32 page_index)
{
    PDFPage page = {0};
    if(!pdf_is_valid(pdf)) return page;

    page.ref = fz_load_page(pdf->ctx, pdf->doc, page_index);
    return page;
}

Image pdf_get_full_image_of_page(PDF *pdf, PDFPage page)
{
    Image img = {0};
    if(!pdf_is_valid) return img;
    if(!page.ref) return img;

    fz_matrix ctm  = fz_identity;
    fz_pixmap *pix = fz_new_pixmap_from_page(pdf->ctx, page.ref, ctm, fz_device_rgb(pdf->ctx), 0);

    s32 w = fz_pixmap_width(pdf->ctx, pix);
    s32 h = fz_pixmap_height(pdf->ctx, pix);
    u32 n = fz_pixmap_components(pdf->ctx, pix); // should be 3

    RGBColor *data = PUSH_ARRAY(pdf->arena, RGBColor, w * h);
    for(u32 y = 0; y < h; ++y)
    {
        u8 *src = pix->samples + y * pix->stride;
        RGBColor *dst = data + y * w;
        for(u32 x = 0; x < w; ++x)
        {
            dst[x].r = src[x * n + 0];
            dst[x].g = src[x * n + 1];
            dst[x].b = src[x * n + 2];
        }
    }

    fz_drop_pixmap(pdf->ctx, pix);

    img.data = data;
    img.props.w = w;
    img.props.h = h;
    img.props.scale = 1.0f;
    img.props_orig = img.props;
    img.arena = pdf->arena;

    return img;
}

void pdf_close_page(PDF *pdf, PDFPage page)
{
    fz_drop_page(pdf->ctx, page.ref);
}

List pdf_read_images_from_page(PDF *pdf, PDFPage page)
{
    List images = list_create(pdf->arena, sizeof(PDFImage));
    if(!pdf_is_valid(pdf)) return images;

    fz_context *ctx = pdf->ctx;
    image_capture_device *dev = fz_new_derived_device(ctx, image_capture_device);
    dev->base.fill_image = callback_image_filler;
    dev->images = &images;
    dev->arena  = pdf->arena;

    fz_run_page(pdf->ctx, page.ref, (fz_device *)dev, fz_identity, NULL);
    fz_drop_device(pdf->ctx, (fz_device *)dev);

    return images;
}

void pdf_write_images_to_page(PDF *pdf, List images, PDFPage page)
{
    if(!pdf_is_valid(pdf)) return;

    pdf_document *pdoc = (pdf_document *)pdf->doc;
    pdf_page *pdf_pg   = (pdf_page *)page.ref;

    pdf_obj *resources = pdf_dict_get(pdf->ctx, pdf_pg->obj, PDF_NAME(Resources));
    pdf_obj *xobjects  = pdf_dict_get(pdf->ctx, resources,   PDF_NAME(XObject));

    // Iterate the XObject dictionary
    s32 xobj_count = pdf_dict_len(pdf->ctx, xobjects);
    s32 img_index  = 0;

    for(s32 j = 0; j < xobj_count; j++)
    {
        pdf_obj *val = pdf_dict_get_val(pdf->ctx, xobjects, j);

        // skip non-image XObjects
        if(!pdf_is_stream(pdf->ctx, val)) continue;

        pdf_obj *subtype = pdf_dict_get(pdf->ctx, val, PDF_NAME(Subtype));
        if(!pdf_name_eq(pdf->ctx, subtype, PDF_NAME(Image))) continue;

        if(img_index >= (s32)images.count) break;
        PDFImage *pdf_img = list_get(&images, img_index++);
        if(pdf_img->total_changes == 0) continue; // keep original

        Image *img = &pdf_img->image;

        fz_buffer *buf = fz_new_buffer(pdf->ctx, img->props.w * img->props.h * 3);
        for(u32 y = 0; y < img->props.h; y++)
        {
            RGBColor *row = img->data + y * img->props.w;
            fz_append_data(pdf->ctx, buf, row, img->props.w * sizeof(RGBColor));
        }

        // Update the stream in-place
        pdf_update_stream(pdf->ctx, pdoc, val, buf, 0);
        fz_drop_buffer(pdf->ctx, buf);

        pdf_dict_put_int(pdf->ctx, val, PDF_NAME(Width),           img->props.w);
        pdf_dict_put_int(pdf->ctx, val, PDF_NAME(Height),          img->props.h);
        pdf_dict_put_int(pdf->ctx, val, PDF_NAME(BitsPerComponent), 8);
        pdf_dict_put_name(pdf->ctx, val, PDF_NAME(ColorSpace),     "DeviceRGB");

        // Remove any stale compression filters
        pdf_dict_del(pdf->ctx, val, PDF_NAME(Filter));
        pdf_dict_del(pdf->ctx, val, PDF_NAME(DecodeParms));
    }
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
    if(!pdf_is_valid(pdf)) return;

    fz_drop_document(pdf->ctx, (fz_document *)pdf->doc);
    fz_drop_context(pdf->ctx);
}

