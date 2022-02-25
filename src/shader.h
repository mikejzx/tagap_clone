#ifndef SHADER_H
#define SHADER_H

/*
 * shader.h
 *
 * Defines shaders used in the game
 */

#define SHADER_NAME_MAX 32
#define MAX_VERTEX_ATTR 4

enum shader_type
{
    SHADER_DEFAULT = 0,
    SHADER_DEFAULT_NO_ZBUFFER, // Default without depth testing
    SHADER_VERTEXLIT,

    // Particle shader; rendered separately from everything else
    SHADER_PARTICLE,

    // Light rendering shader, used only in the light render pass
    SHADER_LIGHT,

    // Special shader only used in subpass 2 to read colour attachment and do
    // basic post-processing
    SHADER_SCREENSUBPASS,

    SHADER_COUNT
};

// Rendering order of shaders
static const u32 SHADER_RENDER_ORDER[SHADER_COUNT] =
{
    SHADER_DEFAULT,
    SHADER_VERTEXLIT,
    SHADER_DEFAULT_NO_ZBUFFER,
    SHADER_PARTICLE,

    // Unordered (as the order only applies to Pass 2, Subpass 2, which these
    //            shaders are not used in)
    SHADER_LIGHT,
    SHADER_SCREENSUBPASS,
};

/* Vertex attributes for default shader */
struct vertex
{
    // Vertex positions
    vec3s pos;

    // Texcoords
    vec2s texcoord;
};

/* Vertex attributes for vertexlit shader */
struct vertex_vl
{
    // Vertex positions
    vec3s pos;

    // Vertex colour
    vec4s colour;
};

// Push constants for default shader
struct push_constants
{
    mat4s mvp;
    vec4s shading;
    vec2s tex_offset;
    int tex_index;
};

// Push constants for vertexlit shader
struct push_constants_vl
{
    // Only need MVP; any shading can be done on vertices directly
    mat4s mvp;
};

/* Vertex attributes for particle shader */
struct vertex_ptl
{
    vec2s pos;
    vec4s colour;
    i32 tex_index;
};

// Push constants for particle shader
struct push_constants_ptl
{
    // Only need MVP; any shading can be done on vertices directly, and
    // texcoords are stored in shader itself as we only render quads with this
    mat4s mvp;
};

// Push constants for light shader
struct push_constants_light
{
    mat4s mvp;
    vec4s colour;
    int tex_index;
};
// Push constants for screenpass shader
struct push_constants_sp2
{
    // Shading colour
    vec4s shade_colour;

    // Environment texture coordinate dilation and offsets
    struct
    {
        vec2s texcoord_mul;
        vec2s texcoord_offset;
    } env;
};

struct shader
{
    char name[SHADER_NAME_MAX];

    // Shader pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    // The push constants structure this shader uses
    size_t pconst_size;

    // Vertex buffer info
    VkVertexInputBindingDescription vertex_binding_desc;
    VkVertexInputAttributeDescription vertex_attr_desc[MAX_VERTEX_ATTR];

    // Whether to use descriptor sets
    bool use_descriptor_sets;

    // Whether to read/write to Z-buffer
    bool depth_test;

    // Whether to use blending
    bool blending;
    bool blend_additive;
};

// Shader list
extern struct shader g_shader_list[SHADER_COUNT];

i32 vulkan_shaders_init_all(void);
i32 vulkan_shaders_free_all(void);

#endif
