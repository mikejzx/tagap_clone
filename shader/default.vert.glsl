#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_Texcoord;

layout(location = 0) out vec2 v_Texcoord;
layout(location = 1) flat out int v_TexIndex;
layout(location = 2) out vec4 v_Shading;

// Push constants block
layout(push_constant) uniform constants
{
    // Model-view-projection matrix
    mat4 mvp;

    // Shading colour; note that we for unshaded things we set this to pure
    // white
    vec4 shading;

    // Index of the texture this thing uses
    int tex_index;
} pconsts;

void main()
{
    v_Texcoord = a_Texcoord;
    gl_Position = pconsts.mvp * vec4(a_Position, 0.0, 1.0);
    v_Shading = pconsts.shading;
    v_TexIndex = pconsts.tex_index;
}
