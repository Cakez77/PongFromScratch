#pragma once

#include "defines.h"

enum KeyID
{
    A_KEY,
    D_KEY,
    S_KEY,
    W_KEY,

    KEY_COUNT
};

struct Key
{
    bool isDown;
    uint32_t halfTransitionCount;
};

struct InputState
{
    Key keys[KEY_COUNT];
};

bool key_pressed_this_frame(InputState* input, KeyID key);
bool key_released_this_frame(InputState* input, KeyID key);
bool key_is_down(InputState* input, KeyID key);
