
// Based on PCG Random Number Generator (https://www.pcg-random.org)
// Licensed under Apache License 2.0

static THREAD_LOCAL RandomGenerator s_randgen = {
    0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL
};

void randgen_seed_r(RandomGenerator *generator, u64 seed, u64 seq)
{
    generator->state = 0U;
    generator->inc = (seq << 1u) | 1u;
    randgen_u32_r(generator);
    generator->state += seed;
    randgen_u32_r(generator);
}

void randgen_seed_with_entropy_r(RandomGenerator *generator)
{
    u64 s1 = 0;
    u64 s2 = 0;

    os_entropy((u8 *)&s1, sizeof(u64));
    os_entropy((u8 *)&s2, sizeof(u64));

    randgen_seed_r(generator, s1, s2);
}

u32 randgen_u32_r(RandomGenerator *generator)
{
    u64 old_state = generator->state;
    generator->state = old_state * 6364136223846793005ULL + generator->inc;
    u32 xor_shifted = ((old_state >> 18u) ^ old_state) >> 27u;
    u32 rot = old_state >> 59u;

    return (xor_shifted >> rot) | (xor_shifted << ((-rot) & 31));
}

f32 randgen_f32_r(RandomGenerator *generator)
{
    return ((f32)randgen_u32_r(generator) / UINT32_MAX);
}

void randgen_seed(u64 seed, u64 seq)
{
    randgen_seed_r(&s_randgen, seed, seq);
}

void randgen_seed_with_entropy(void)
{
    randgen_seed_with_entropy_r(&s_randgen);
}

u32 randgen_u32(void)
{
    return randgen_u32_r(&s_randgen);
}

f32 randgen_f32(void)
{
    return randgen_f32_r(&s_randgen);
}
