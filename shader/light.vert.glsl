#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_Texcoord;

layout(location = 0) out vec2 v_Texcoord;
layout(location = 1) out vec4 v_Colour;
layout(location = 2) flat out int v_TexIndex;

// Push constants block
layout(push_constant) uniform constants
{
    mat4 mvp;
    vec4 colour;
    int tex_index;
} pconsts;

void main()
{
    v_Texcoord = a_Texcoord;
    gl_Position = pconsts.mvp * vec4(a_Position, 1.0);
    v_Colour = pconsts.colour;
    v_TexIndex = pconsts.tex_index;
}
