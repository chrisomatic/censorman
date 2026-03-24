#pragma once

//===================================
// Includes
//===================================

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <time.h>

//===================================
// Third-Party Includes
//===================================

#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_STATIC
#include "stb_sprintf.h"

//===================================
// Types
//===================================

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef int8_t   b8;
typedef int16_t  b16;
typedef int32_t  b32;
typedef int64_t  b64;

typedef struct
{
    b32 success;
    u32 err_code;
} FuncResult;

typedef struct
{
    u64 len;
    u8 *data;
} ByteArray;

//===================================
// Utility Helpers
//===================================

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define DEBUG() logd("!! %s, L%d", __func__, __LINE__)
#define SWAP(T, a, b) do { T temp = a; a = b; b = temp; } while (0)
#define BOOLSTR(b) (b) ? "True" : "False"

#define KB(n)  (((u64)(n)) << 10)
#define MB(n)  (((u64)(n)) << 20)
#define GB(n)  (((u64)(n)) << 30)
#define TB(n)  (((u64)(n)) << 40)

#define ALIGN_UP_POW2(x,b) (((x) + (b) - 1) & (~((b) - 1)))

//===================================
// Bit-wise Helpers
//===================================

#define BIT_SET(base,n)    ((base) |= (1UL<<(n)))
#define BIT_CLR(base,n)    ((base) &= ~(1UL<<(n)))
#define BIT_FLIP(base,n)   ((base) ^= (1UL<<(n)))
#define BIT_CHECK(base,n)  (((base) & (n)) == (n))
#define BIT_IS_SET(base,n) ((base) & (1UL<<(n)))

//===================================
// Memory Helpers
//===================================

#define MemoryCompare(a, b, size)  memcmp((a), (b), (size))
#define MemoryZero(p,z)            memset((p), 0, (z))
#define MemoryZeroStruct(p)        MemoryZero((p), sizeof(*(p)))
#define MemoryCopy(d,s,z)          memmove((d), (s), (z))
#define MemoryCopyStruct(d,s)      MemoryCopy((d), (s), MIN(sizeof(*(d)), sizeof(*(s))))
#define MemorySet(dst, byte, size) memset((dst), (byte), (size))

