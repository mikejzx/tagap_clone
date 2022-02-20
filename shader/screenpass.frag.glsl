#version 450
#extension GL_ARB_separate_shader_objects : enable

// Colour input attachment
layout(input_attachment_index = 0, binding = 0) uniform subpassInput i_Colour;
// Lightmap
layout(binding = 1) uniform sampler2D u_Lightmap;

layout(location = 0) in vec4 v_Shading;
layout(location = 1) in vec2 v_Texcoord;

layout(location = 0) out vec4 o_FragColour;

void main()
{
    vec3 light = texture(u_Lightmap, v_Texcoord).rgb;
    o_FragColour = vec4(
        subpassLoad(i_Colour).rgb * (v_Shading.rgb + light * 10.0),
        1.0f) + vec4(light / 2.5, 0.0);
    //o_FragColour.rgb = light;
}
