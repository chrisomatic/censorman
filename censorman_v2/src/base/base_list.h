#pragma once

typedef struct
{
    void *items;
    u64 item_size;
    u64 count;
} ListArray;

typedef struct ListNode ListNode;
struct ListNode
{
    void *item;
    ListNode *prev;
    ListNode *next;
};

typedef struct
{
    ListNode *head;
    ListNode *last;
    u64 item_size;
    u64 count;
    Arena *arena;
} List;

typedef s32 (*ListCompareFn)(void *a, void *b);

List list_nil();
List list_create(Arena *arena, u64 item_size);

void list_add(List *list, void *item);
void list_add_list(List *list, List *add);
b32 list_remove(List *list, u64 index);
void *list_get(List *list, u64 index);
void list_clear(List *list);

ListArray list_to_array(List *list);
void list_print(List *list);

// sorting
void list_array_sort(ListArray *arr, ListCompareFn cmp);

// example compare functions
s32 list_compare_fn_s32_asc(void *a, void *b);
s32 list_compare_fn_s32_desc(void *a, void *b);
