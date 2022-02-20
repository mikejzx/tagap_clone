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

    // Compose colour buffer with light buffer
    o_FragColour = vec4(
        subpassLoad(i_Colour).rgb * (v_Shading.rgb + light * 20.0),
        1.0f) + vec4(light / 4.0, 0.0);

    // The power here seems to be necessary to get the colours looking right.
    // Perhaps it's controlled by theme DARKNESS variable?  1.2 looks good for
    // the Warehouse level.
    o_FragColour.rgb = pow(o_FragColour.rgb, vec3(1.2f));

    //o_FragColour.rgb = light;
}
