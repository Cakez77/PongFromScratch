#pragma once

#include <cstdint>

#define KB(x) ((uint64_t)x * 1024)
#define MB(x) ((uint64_t)x * KB(1))
#define GB(x) ((uint64_t)x * MB(1))

// #define internal static
#define global_variable static