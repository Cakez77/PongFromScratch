#pragma once

#include <cstdint>

#define KB(x) ((uint64_t)1024 * x)
#define MB(x) ((uint64_t)1024 * KB(1))
#define GB(x) ((uint64_t)1024 * MB(1))

#define BIT(i) (1 << i)

#define ArraySize(arr) sizeof((arr)) / sizeof((arr[0]))

#define INVALID_IDX UINT32_MAX
#define INVALID_NUMBER INT32_MAX

#define line_id(x) (size_t)((__LINE__ << 16) | x)

#define internal static
#define global_variable static