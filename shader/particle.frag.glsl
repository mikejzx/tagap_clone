#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 v_Texcoord;
layout(location = 1) in vec4 v_VertexColour;
layout(location = 2) flat in uint v_TexIndex;

layout(location = 0) out vec4 o_FragColour;

layout(binding = 0) uniform sampler u_Sampler;
layout(binding = 1) uniform texture2D u_Textures[128];

void main()
{
    // This may seem like an absolutely bizarre piece of code, but for some
    // reason this ridiculous loop is necessary to prevent some incredibly
    // strange artifacts (spent hours upon hours trying to debug), which are
    // apparently caused by "dynamic indexing", though I'm not sure how this
    // loop is any less "dynamic"...
    vec4 tex = vec4(0.0, 0.0, 0.0, 1.0);
    for (int i = 0; i < 128; ++i)
    {
        if (i == v_TexIndex)
        {
            tex = texture(sampler2D(u_Textures[i], u_Sampler), v_Texcoord);
        }
    }

    o_FragColour = tex * v_VertexColour;
}
