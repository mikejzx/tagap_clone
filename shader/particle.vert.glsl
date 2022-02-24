#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_VertexColour;
layout(location = 2) in uint a_TexIndex;

layout(location = 0) out vec2 v_Texcoord;
layout(location = 1) out vec4 v_VertexColour;
layout(location = 2) flat out uint v_TexIndex;

// Push constants block
layout(push_constant) uniform constants
{
    // Model-view-projection matrix
    mat4 mvp;
} pconsts;

// Embed the texture coordinates because we only use this shader for drawing
// quads
const vec2 texcoords[4] = vec2[]
(
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, 0.0)
);

void main()
{
    v_Texcoord = texcoords[gl_VertexIndex % 4];
    gl_Position = pconsts.mvp * vec4(a_Position, 0.0, 1.0);
    v_VertexColour = a_VertexColour;
    v_TexIndex = a_TexIndex;
}
