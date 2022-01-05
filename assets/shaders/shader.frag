#version 450

#include "../../src/renderer/shared_render_types.h"

layout(location = 0) in vec2 uv;
layout(location = 1) in flat uint materialIdx;       

layout(location = 0) out vec4 fragmentColor;

layout(set = 0, binding = 2) uniform sampler2D sprite;

layout(set = 0, binding = 3) readonly buffer Materials
{
    MaterialData materials[];
};

void main()
{
    vec4 color = texture(sprite, uv);

    if(color.a == 0)
    {
        discard;
    }

    fragmentColor = color * materials[materialIdx].color;
}