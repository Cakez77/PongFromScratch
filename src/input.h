#pragma once

#include "defines.h"
#include "my_math.h"

enum KeyID
{
    KEY_LEFT_MOUSE,
    KEY_MIDDLE_MOUSE,
    KEY_RIGHT_MOUSE,

    KEY_A,
    KEY_D,
    KEY_S,
    KEY_W,

    KEY_COUNT
};

struct Key
{
    bool isDown;
    uint32_t halfTransitionCount;
};

struct InputState
{
    Vec2 mousePos;
    Vec2 mouseClickPos;
    Vec2 prevMousePos;

    Vec2 screenSize;
    Key keys[KEY_COUNT];
};

bool key_pressed_this_frame(InputState* input, KeyID key);
bool key_released_this_frame(InputState* input, KeyID key);
bool key_is_down(InputState* input, KeyID key);
