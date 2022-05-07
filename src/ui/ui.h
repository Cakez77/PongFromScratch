#pragma once

#include "my_math.h"

uint32_t constexpr MAX_LABELS = 100;

struct Label
{
    char* text;
    Vec2 pos;
};

struct UIState
{
    uint32_t labelCount;
    Label labels[MAX_LABELS];
};

void update_ui(UIState* ui);

void do_text(UIState* ui, Vec2 pos, char* text);