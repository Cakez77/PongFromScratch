#pragma once

#include "logger.h"

struct Vec2
{
    float x;
    float y;

    Vec2 operator/(float scalar)
    {
        return Vec2{x / scalar, y / scalar};
    }

    Vec2 operator*(float scalar)
    {
        return Vec2{x * scalar, y * scalar};
    }

    Vec2 operator+(Vec2 other)
    {
        return Vec2{x + other.x, y + other.y};
    }

    Vec2 operator-(Vec2 other)
    {
        return Vec2{x - other.x, y - other.y};
    }
};

bool line_intersection(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Vec2 *collisionPoint)
{
    CAKEZ_ASSERT(collisionPoint, "No collision point supplied!");

    // Vector from a to b
    Vec2 r = (b - a);

    // Vector from c to d
    Vec2 s = (d - c);

    // Dot Product with (r dot s)
    float d1 = r.x * s.y - r.y * s.x;
    float u = ((c.x - a.x) * r.y - (c.y - a.y) * r.x) / d1;
    float t = ((c.x - a.x) * s.y - (c.y - a.y) * s.x) / d1;

    // If they intersect, return where
    if (0 <= u && u <= 1 && 0 <= t && t <= 1)
    {
        *collisionPoint = a + r * t;
        return true;
    }
    return false;
}

struct Rect
{
    union
    {
        Vec2 pos;
        Vec2 offet;
    };
    Vec2 size;
};

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

float clamp(float value, float a, float b)
{
    if (value < a)
    {
        return a;
    }
    else if (value > b)
    {
        return b;
    }
    else
    {
        return value;
    }
}