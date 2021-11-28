#pragma once

#include "defines.h"

void platform_get_window_size(uint32_t* width, uint32_t*heigth);

char* platform_read_file(char* path, uint32_t* length);
