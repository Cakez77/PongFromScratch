#pragma once

#include <cstdint>

#define KB(x) ((uint64_t)1024 * x)
#define MB(x) ((uint64_t)1024 * KB(1))
#define GB(x) ((uint64_t)1024 * MB(1))

// #define internal static
#define global_variable static