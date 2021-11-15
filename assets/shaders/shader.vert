#version 450

vec2 vertices[3] = {
    vec2(-0.5, 0.5),
    vec2(0.0, -0.5),
    vec2(0.5, 0.5)
};

void main()
{
    gl_Position = vec4(vertices[gl_VertexIndex], 0.5, 1.0);
}