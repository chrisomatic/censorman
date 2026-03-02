#pragma once

typedef enum
{
    DETECT_TYPE_NONE = 0,
    DETECT_TYPE_FACE,
    DETECT_TYPE_PERSON,
    DETECT_TYPE_LICENSE_PLATE,
    DETECT_TYPE_DOCUMENT,
} DetectType;

typedef struct
{
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} Box;

typedef struct
{
    BoxNode *head;
    BoxNode *last;
    u64 count;
} BoxList;

void *detect(void *args);
