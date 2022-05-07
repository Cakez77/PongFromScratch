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
    float topV;
    float bottomV;
    float leftU;
    float rightU;
    int materialIdx;
    // Padding??
};

struct PushData
{
    int transformIdx;
};

struct MaterialData
{
    vec4 color;
};