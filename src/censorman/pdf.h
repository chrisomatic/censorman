#pragma once

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

typedef struct
{
    Image image;
    s32 total_changes;
} PDFImage;

typedef struct
{
    fz_page *ref;
} PDFPage;

typedef struct
{
    fz_context *ctx;
    fz_document *doc;

    s32 page_count;
    Arena *arena;
} PDF;

PDF pdf_open(Arena *arena, String file_path);
b32 pdf_is_valid(PDF *pdf);

PDFPage pdf_open_page(PDF *pdf, s32 page_index);
void pdf_close_page(PDF *pdf, PDFPage page);

List pdf_read_images_from_page(PDF *pdf, PDFPage page);
void pdf_write_images_to_page(PDF *pdf,  List images, PDFPage page);
Image pdf_get_full_image_of_page(PDF *pdf, PDFPage page);

void pdf_save(PDF *pdf, String pdf_path);
void pdf_close(PDF *pdf);

