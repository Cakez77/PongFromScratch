#version 450

#include "../../src/renderer/shared_render_types.h"

layout (location = 0) out vec2 uv;
layout (location = 1) out uint materialIdx;

layout(set = 0, binding = 0) uniform GlobalUBO
{
    GlobalData globalData;
};

layout(set = 0, binding = 1) readonly buffer Transforms
{
    Transform transforms[];
};

layout(push_constant) uniform block
{
    PushData pushData;
};

Transform transform = transforms[pushData.transformIdx + gl_InstanceIndex];

vec4 vertices[4] = {
    // Top Left 
    vec4(transform.xPos, transform.yPos, 0.0, 0.0),

    // Bottom Left
    vec4(transform.xPos, transform.yPos + transform.sizeY, 0.0, 1.0),

    // Bottom Right
    vec4(transform.xPos + transform.sizeX, transform.yPos + transform.sizeY, 1.0, 1.0),

    // Top Right
    vec4(transform.xPos + transform.sizeX, transform.yPos, 1.0, 0.0),
};

void main()
{
    vec2 normalizedPos =  2.0 * vec2(vertices[gl_VertexIndex].x / globalData.screenSizeX, vertices[gl_VertexIndex].y / globalData.screenSizeY) - 1.0;
    gl_Position = vec4(normalizedPos, 1.0, 1.0);
    uv = vertices[gl_VertexIndex].zw;
    materialIdx = transform.materialIdx;
}