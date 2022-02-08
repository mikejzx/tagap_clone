#version 450

layout(location = 0) in vec2 i_Texcoord;

layout(location = 0) out vec4 o_Colour;

layout(binding = 0) uniform sampler2D v_Sampler;

void main()
{
    o_Colour = texture(v_Sampler, i_Texcoord);
    //o_Colour = vec4(i_Texcoord, 0.0, 1.0);
}
