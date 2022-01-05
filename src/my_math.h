#pragma once

struct Vec4
{
    union
    {
        struct
        {
            float r;
            float g;
            float b;
            float a;
        };
        struct
        {
            float x;
            float y;
            float z;
            float w;
        };
    };

    bool operator==(Vec4 other)
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};