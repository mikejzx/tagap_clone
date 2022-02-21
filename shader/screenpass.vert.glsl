#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex
{
    vec4 gl_Position;
};

// Push constants block
layout(push_constant) uniform constants
{
    // Level theme shading colour
    vec4 shading;

    // Offset of environment texture
    vec4 env_tex_offsets;
} pconsts;

layout(location = 0) out vec4 v_Shading;
layout(location = 1) out vec2 v_Texcoord;
layout(location = 2) out vec4 v_EnvTexcoords;

void main()
{
    // From Sascha Williems 'inputattachments' example
    gl_Position = vec4(
        vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2) * 2.0f - 1.0f,
        0.0f, 1.0f);
    v_Texcoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    v_Shading = pconsts.shading;
    v_EnvTexcoords = pconsts.env_tex_offsets;
}
