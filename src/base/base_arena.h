#pragma once

#define PUSH_ONE(arena,T)    arena_push((arena), sizeof(T), false)
#define PUSH_ONE_NZ(arena,T) arena_push((arena), sizeof(T), true)

#define PUSH_ARRAY(arena,T,n)    arena_push((arena), (n) * sizeof(T), false)
#define PUSH_ARRAY_NZ(arena,T,n) arena_push((arena), (n) * sizeof(T), true)

typedef struct Arena Arena;
struct Arena
{
    u8  *memory;        // pointer to beginning of memory block
    u64 capacity;       // the total size of the memory block
    u64 offset;         // current offset in memory block
    u64 base_pos;       // absolute position for this arena's memory
    Arena *next;        // used for chaining arenas together
};

typedef struct
{
    Arena *arena;
    u64 start_pos;
} ArenaTemp;

typedef ArenaTemp Temp;

// global arena for convenience
extern Arena scratch;

Arena *arena_create(u64 capacity);
void arena_destroy(Arena* arena);

void* arena_push(Arena *arena, u64 size, b32 non_zero);
void arena_pop_to(Arena *arena, u64 pos);

void arena_reset(Arena* arena);
u64 arena_pos(Arena *arena);

ArenaTemp arena_temp_begin(Arena *arena);
void arena_temp_end(ArenaTemp temp);

// to get scratch arenas
Temp scratch_begin(void);
void scratch_end(Temp scratch);
