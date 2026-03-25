
s32 list_compare_fn_s32_desc(void *a, void *b)
{
    s32 *ia = (s32 *)a;
    s32 *ib = (s32 *)b;

    return (*ia < *ib) - (*ia > *ib);
}

s32 list_compare_fn_s32_asc(void *a, void *b)
{
    s32 *ia = (s32 *)a;
    s32 *ib = (s32 *)b;

    return (*ia > *ib) - (*ia < *ib);
}

List list_nil(void)
{
    List list = {0};
    return list;
}

List list_create(Arena *arena, u64 item_size)
{
    List list = {0};

    list.head = NULL;
    list.last = NULL;
    list.count = 0;
    list.item_size = item_size;
    list.arena = arena;

    return list;
}

void list_add(List *list, void *item)
{
    if(!item) return;

    if(list->head == NULL)
    {
        // first element
        list->head = PUSH_ONE(list->arena, ListNode);
        list->head->prev = NULL;
        list->head->next = NULL;
        list->last = list->head;
    }
    else
    {
        ListNode *prior_last = list->last;
        list->last->next = PUSH_ONE(list->arena, ListNode);
        list->last = list->last->next;
        list->last->next = NULL;
        list->last->prev = prior_last;
    }

    list->last->item = arena_push(list->arena, list->item_size, true);
    MemoryCopy(list->last->item, item, list->item_size);
    list->count++;
}

void list_add_list(List *list, List *add)
{
    for(s64 i = 0; i < add->count; ++i)
    {
        list_add(list, list_get(add, i));
    }
}

b32 list_remove(List *list, u64 index)
{
    ListNode *ln = list->head;
    for(s64 idx = 0; ln && idx < index; ++idx)
        ln = ln->next;

    if(!ln) return false;

    if(ln->prev) ln->prev->next = ln->next;
    else         list->head     = ln->next;

    if(ln->next) ln->next->prev = ln->prev;
    else         list->last     = ln->prev;

    ln->prev = NULL;
    ln->next = NULL;
    list->count--;

    return true;
}

void *list_get(List *list, u64 index)
{
    if(index >= list->count)
        return NULL;

    u64 idx = 0;

    ListNode *ln = list->head;

    for(;;)
    {
        if(idx > index)
            break;

        if(idx == index)
            return ln->item;

        if(!ln->next)
            break;

        ln = ln->next;
        idx++;
    }

    return NULL;
}

void list_clear(List *list)
{
    list->head = NULL;
    list->last = NULL;
    list->count = 0;
}

ListArray list_to_array(List *list)
{
    ListArray arr = {0};

    arr.items = arena_push(list->arena, list->count * list->item_size, false);

    u64 total_size = 0;
    ListNode *ln = list->head;
    if(!ln) return arr;

    for(;;)
    {
        void *it = ln->item;
        u8 *items = (u8 *)arr.items;
        if(it) MemoryCopy(&items[total_size], it, list->item_size);
        total_size += list->item_size;

        if(!ln->next)
            break;

        ln = ln->next;
    }

    arr.item_size = list->item_size;
    arr.count = list->count;

    return arr;
}

void list_print(List *list)
{
    ArenaTemp temp = arena_temp_begin(list->arena);

    StringList sl = string_list_create(temp.arena);
    string_list_addf(&sl, "List %p (count: %d):\n[\n", list, list->count);

    ListNode *ln = list->head;
    if(!ln) return;

    u64 item_count = 0;

    for(;;)
    {
        void *it = ln->item;
        if(it) 
        {
            switch(list->item_size)
            {
                case 1:
                {
                    s8 *x = (s8 *)it;
                    string_list_addf(&sl, "  %lu: [ %d ]\n", item_count, *x);
                } break;
                case 2:
                {
                    s16 *x = (s16 *)it;
                    string_list_addf(&sl, "  %lu: [ %d ]\n", item_count, *x);
                } break;
                case 4:
                {
                    s32 *x = (s32 *)it;
                    string_list_addf(&sl, "  %lu: [ %d ]\n", item_count, *x);
                } break;
                case 8:
                {
                    s64 *x = (s64 *)it;
                    string_list_addf(&sl, "  %lu: [ %ld ]\n", item_count, *x);
                } break;
                default:
                {
                    string_list_addf(&sl, "  %lu: [ ", item_count);
                    for(s32 i = 0; i < list->item_size; ++i)
                    {
                        u8 *x = (u8 *)it;
                        string_list_addf(&sl, "%02X ", *(x+i));
                    }
                    string_list_add(&sl, S("]\n"));
                }
            }
        }

        if(!ln->next)
            break;

        ln = ln->next;
        item_count++;
    }

    string_list_add(&sl, S("]\n"));
    string_print(string_list_collapse(&sl));

    arena_temp_end(temp);
}

void list_array_sort(ListArray *arr, ListCompareFn cmp)
{
    // insertion sort
    u8 *items = (u8 *)arr->items;
    u64 es = arr->item_size;

    u8 tmp[512]; // small buffer that should support "item_size"

    for(s64 i = 1; i < arr->count; ++i)
    {
        s64 j = i;
        for(; j > 0 && cmp(items + (j-1)*es, items + j*es) > 0; j--)
        {
            u8 *a = items + j*es;
            u8 *b = items + (j-1)*es;

            // swap
            MemoryCopy(tmp, a,   es);
            MemoryCopy(a,   b,   es);
            MemoryCopy(b,   tmp, es);
        }
    }
}
