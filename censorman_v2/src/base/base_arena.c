//===================================
// Arenas
//===================================

#define ARENA_ALIGN sizeof(void*)

// global scratch arena
static Arena *_scratch_arena = {0};

Arena *arena_create(u64 capacity)
{
    Arena *a = (Arena *)malloc(sizeof(Arena));

    a->memory = (u8*)malloc(capacity);
    a->capacity = capacity;
    a->offset = 0;
    a->base_pos = 0;
    a->next = NULL;

    return a;
}

void arena_destroy(Arena *arena)
{
    if(!arena) return;

    for(;;)
    {
        if(arena->memory) free(arena->memory);

        arena->memory = NULL;
        arena->capacity = 0;
        arena->offset = 0;
        arena->base_pos = 0;

        if(arena->next)
        {
            Arena* tmp = arena;
            arena = arena->next;
            free(tmp);
            continue;
        }

        break;
    }

    arena = NULL;
}

void* arena_push(Arena *arena, u64 size, b32 non_zero)
{
    assert(arena);

    u64 offset_aligned = ALIGN_UP_POW2(arena->offset, ARENA_ALIGN);

    for(;;)
    {
        if(offset_aligned + size <= arena->capacity)
            break; // enough space, we're good

        // can't fit data on current arena
        // check for a next arena
        if(arena->next)
        {
            arena = arena->next;
            continue;
        }

        // allocate a new arena that doubles the arena memory capacity
        // or more to accommodate a large allocation
        
        u64 new_arena_size = (arena->capacity >= size ? 2*arena->capacity : size);

        logv("Increasing arena size from %u to %u!", arena->capacity, new_arena_size);

        arena->next = (Arena*)malloc(sizeof(Arena));
        arena->next->memory = (u8*)malloc(new_arena_size * sizeof(u8));
        arena->next->offset = 0;
        arena->next->base_pos = arena->base_pos + arena->capacity;
        arena->next->capacity = new_arena_size;
        arena->next->next = NULL;
    }

    void *ptr = arena->memory + offset_aligned;
    arena->offset = offset_aligned + size;

    if(!non_zero) MemoryZero(ptr, size);

    return ptr;
}

void arena_pop_to(Arena *arena, u64 pos)
{
    for(;;)
    {
        if(BETWEEN(pos, arena->base_pos, arena_pos(arena)))
        {
            arena->offset = (pos - arena->base_pos);
        }
        else if(pos < arena->base_pos)
        {
            arena->offset = 0;
        }

        if(arena->next)
        {
            arena = arena->next;
            continue;
        }
        break;
    }
}

u64 arena_pos(Arena *arena)
{
    return arena->base_pos + arena->offset;
}

ArenaTemp arena_temp_begin(Arena *arena)
{
    return (ArenaTemp){.arena = arena, .start_pos = arena_pos(arena)};
}

void arena_temp_end(ArenaTemp temp)
{
    arena_pop_to(temp.arena, temp.start_pos);
}

ArenaTemp scratch_begin(void)
{
    if(_scratch_arena == NULL)
    {
        _scratch_arena = arena_create(MB(64));
    }

    return arena_temp_begin(_scratch_arena);

}

void scratch_end(ArenaTemp scratch)
{
    arena_temp_end(scratch);
}


void arena_reset(Arena *arena)
{
    for(;;)
    {
        arena->offset = 0;

        if(arena->next)
        {
            arena = arena->next;   
            continue;
        }
        break;
    }
}
