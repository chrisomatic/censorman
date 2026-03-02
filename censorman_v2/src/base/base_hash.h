#pragma once

u32 hash_data(const void* data, u64 len, u32 seed);
u32 hash_string(String str, u32 seed);
