
// Based on PCG Random Number Generator (https://www.pcg-random.org)
// Licensed under Apache License 2.0

static THREAD_LOCAL RandomContext g_random_context = {
    0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL
};

void random_seed_r(RandomContext *context, u64 seed, u64 seq)
{
    context->state = 0U;
    context->inc = (seq << 1u) | 1u;
    random_r(context);
    context->state += seed;
    random_r(context);
}

void random_seed_with_entropy_r(RandomContext *context)
{
    u64 s1 = 0;
    u64 s2 = 0;

    os_entropy((u8 *)&s1, sizeof(u64));
    os_entropy((u8 *)&s2, sizeof(u64));

    random_seed_r(context, s1, s2);
}

u32 random_r(RandomContext *context)
{
    u64 old_state = context->state;
    context->state = old_state * 6364136223846793005ULL + context->inc;
    u32 xor_shifted = ((old_state >> 18u) ^ old_state) >> 27u;
    u32 rot = old_state >> 59u;

    return (xor_shifted >> rot) | (xor_shifted << ((-rot) & 31));
}

void random_seed(u64 seed, u64 seq)
{
    random_seed_r(&g_random_context, seed, seq);
}

void random_seed_with_entropy(void)
{
    random_seed_with_entropy_r(&g_random_context);
}

u32 random(void)
{
    return random_r(&g_random_context);
}

f32 random_float_r(RandomContext *context)
{
    return ((f32)random_r(context) / UINT32_MAX);
}

f32 random_float(void)
{
    return random_float_r(&g_random_context);
}
