
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

s32 pdf_open(Arena *arena, String file_path)
{
    fz_context *ctx = NULL;
    pdf_document *pdoc = NULL;
    s32 rc = 0;

    ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
    if(!ctx)
    {
        loge("Failed to create MuPDF context");
        return -1;
    }

    fz_register_document_handlers(ctx);

    char* file_path_cstr = string_to_cstr(arena, file_path);
    fz_try(ctx)
    {
        pdoc = pdf_open_document(ctx, file_path_cstr);
        s32 page_count = pdf_count_pages(ctx, pdoc);
        logi("Opened '%s': %d page(s)", file_path_cstr, page_count);
    }
    fz_always(ctx)
    {
        pdf_drop_document(ctx, pdoc);
        fz_drop_context(ctx);
    }
    fz_catch(ctx)
    {
        loge("Fatal: %s", fz_caught_message(ctx));
        rc = -1;
    }

    return rc;
}
