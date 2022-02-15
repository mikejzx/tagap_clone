#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_VertexColour;

layout(location = 0) out vec4 v_FragColour;

// Push constants block
layout(push_constant) uniform constants
{
    // Model-view-projection matrix
    mat4 mvp;
} pconsts;

void main()
{
    gl_Position = pconsts.mvp * vec4(a_Position, 1.0);
    v_FragColour = a_VertexColour;
}
