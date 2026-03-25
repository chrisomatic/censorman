//===================================
// Arenas
//===================================

#define ARENA_HEADER_SIZE sizeof(Arena)
#define ARENA_ALIGN sizeof(void*)

// global scratch arena
static THREAD_LOCAL Arena *_scratch_arena = {0};

Arena *arena_create(u64 capacity)
{
    u8 *memory = malloc(capacity);
    MemoryZero(memory,capacity);

    Arena *a = (Arena *)memory;

    a->memory = memory;
    a->capacity = capacity;
    a->offset = ARENA_HEADER_SIZE;
    a->base_pos = 0;
    a->next = NULL;

    return a;
}

void arena_destroy(Arena *arena)
{
    if(!arena) return;

    for(;;)
    {
        if(arena->memory)
            free(arena->memory);

        if(arena->next)
        {
            arena = arena->next;
            continue;
        }

        break;
    }
}

void* arena_push(Arena *arena, u64 size, b32 non_zero)
{
    assert(arena);

    u32 chain_count = 0;

    for(;;)
    {
        u64 offset_aligned = ALIGN_UP_POW2(arena->offset, ARENA_ALIGN);

        if(offset_aligned + size <= arena->capacity)
        {
            // enough space, we're good
            void *ptr = arena->memory + offset_aligned;
            arena->offset = offset_aligned + size;

            if(!non_zero) MemoryZero(ptr, size);
            return ptr;
        }

        // can't fit data on current arena
        // check for a next arena
        if(arena->next)
        {
            chain_count++;
            arena = arena->next;
            continue;
        }

        // allocate a new arena that doubles the arena memory capacity
        // or more to accommodate a large allocation

        u64 new_arena_size = (arena->capacity >= size ? 1.5*arena->capacity : size);
        arena->next = arena_create(new_arena_size);
        arena->next->base_pos = arena->base_pos + arena->capacity;

        logv("Increasing arena (%p, chain: %d) size from %u to %u!", arena, chain_count, arena->capacity, new_arena_size);
    }
}

void arena_pop_to(Arena *arena, u64 pos)
{
    for(;;)
    {
        if(BETWEEN(pos, arena->base_pos, arena_pos(arena)))
        {
            u64 new_offset = pos - arena->base_pos;
            arena->offset = MAX(new_offset, ARENA_HEADER_SIZE); // don't clobber header
        }
        else if(pos < arena->base_pos)
        {
            arena->offset = ARENA_HEADER_SIZE;
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
        arena->offset = ARENA_HEADER_SIZE;

        if(arena->next)
        {
            arena = arena->next;   
            continue;
        }
        break;
    }
}
