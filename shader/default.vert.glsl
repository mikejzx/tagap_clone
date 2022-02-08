#version 330
#extension GL_ARB_separate_shader_objects : enable

// Push constants block
layout(push_constant) uniform constants
{
    mat4 mvp;
} pconsts;

layout(location = 0) in vec2 a_Position;

void main()
{
    gl_Position = pconsts.mvp * vec4(a_Position, 0.0, 1.0);
}
