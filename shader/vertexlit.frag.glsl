#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 v_FragColour;

layout(location = 0) out vec4 o_FragColour;

void main()
{
    o_FragColour = v_FragColour;
}
