#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragmentColor;

layout(set = 0, binding = 0) uniform sampler2D sprite;

void main()
{
    vec4 color = texture(sprite, uv);

    if(color.a == 0)
    {
        discard;
    }

    fragmentColor = color;
}