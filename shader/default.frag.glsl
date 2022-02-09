#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 v_Texcoord;
layout(location = 1) flat in int v_TexIndex;

layout(location = 0) out vec4 o_FragColour;

layout(binding = 0) uniform sampler u_Sampler;
layout(binding = 1) uniform texture2D u_Textures[128];

void main()
{
    o_FragColour = texture(
        sampler2D(u_Textures[v_TexIndex], u_Sampler),
        v_Texcoord);
    //o_FragColour = vec4(v_Texcoord, 0.0, 1.0);
}
