#pragma once

typedef struct
{
    u64 state;
    u64 inc;
} RandomGenerator;

void randgen_seed_r(RandomGenerator *generator, u64 seed, u64 seq);
void randgen_seed_with_entropy_r(RandomGenerator *generator);
u32 randgen_u32_r(RandomGenerator *generator);
f32 randgen_f32_r(RandomGenerator *generator);

void randgen_seed(u64 seed, u64 seq);
void randgen_seed_with_entropy(void);
u32 randgen_u32(void);
f32 randgen_f32(void);
