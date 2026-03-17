#pragma once

typedef struct
{
    u64 state;
    u64 inc;
} RandomContext;

// uses any random context
void random_seed_r(RandomContext *context, u64 seed, u64 seq);

u32 random_r(RandomContext *context);
f32 random_float_r(RandomContext *context);

// uses global random context
void random_seed(u64 seed, u64 seq);
void random_seed_with_entropy_r(RandomContext *context);
void random_seed_with_entropy(void);

u32 random(void);
f32 random_float(void);

