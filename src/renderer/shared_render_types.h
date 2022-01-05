#ifdef CAKEZGINE
#pragma once
#include "my_math.h"
#define vec4 Vec4
#endif

struct GlobalData
{
    int screenSizeX;
    int screenSizeY;
};

struct Transform
{
    float xPos;
    float yPos;
    float sizeX;
    float sizeY;
    int materialIdx;
};

struct PushData
{
    int transformIdx;
};

struct MaterialData
{
    vec4 color;
};