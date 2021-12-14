#pragma once

#include "defines.h"

enum TextColor{
    TEXT_COLOR_WHITE,
    TEXT_COLOR_GREEN,
    TEXT_COLOR_YELLOW,
    TEXT_COLOR_RED,
    TEXT_COLOR_LIGHT_RED,
};

void platform_get_window_size(uint32_t* width, uint32_t*heigth);

char* platform_read_file(char* path, uint32_t* length);

void platform_log(const char* msg, TextColor color);
