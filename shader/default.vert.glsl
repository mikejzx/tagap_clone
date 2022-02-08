#version 450
#extension GL_ARB_separate_shader_objects : enable

// Push constants block
layout(push_constant) uniform constants
{
    mat4 mvp;
} pconsts;

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_Texcoord;

layout(location = 0) out vec2 o_Texcoord;

void main()
{
    gl_Position = pconsts.mvp * vec4(a_Position, 0.0, 1.0);
    o_Texcoord = a_Texcoord;
}
