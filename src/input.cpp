#include "input.h"

bool key_pressed_this_frame(InputState *input, KeyID key)
{
    // TODO: Out of bounds check?
    Key *k = &input->keys[key];
    bool result = k->isDown && k->halfTransitionCount == 1 || k->halfTransitionCount > 1;
    return result;
}

bool key_released_this_frame(InputState *input, KeyID key)
{
    Key *k = &input->keys[key];
    bool result = !k->isDown && k->halfTransitionCount == 1 || k->halfTransitionCount > 1;
    return result;
}

bool key_is_down(InputState *input, KeyID key)
{
    return input->keys[key].isDown;
}