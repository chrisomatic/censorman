#pragma once

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

typedef struct
{
    fz_context *ctx;
    fz_document *doc;

    s32 page_count;
    Arena *arena;
} PDF;

PDF pdf_open(Arena *arena, String file_path);
b32 pdf_is_valid(PDF *pdf);

void pdf_save(PDF *pdf, String pdf_path);
void pdf_close(PDF *pdf);
