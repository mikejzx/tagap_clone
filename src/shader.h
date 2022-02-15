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
    SHADER_VERTEXLIT,

    SHADER_COUNT
};

/* Vertex attributes for default shader */
struct vertex
{
    // Vertex positions
    vec2s pos;

    // Texcoords
    vec2s texcoord;
};

/* Vertex attributes for vertexlit shader */
struct vertex_vl
{
    // Vertex positions
    vec2s pos;

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
    u32 vertex_attr_count;

    // Whether to use descriptor sets
    bool use_descriptor_sets;
};

// Shader list
extern struct shader g_shader_list[SHADER_COUNT];

i32 vulkan_shaders_init_all(void);
i32 vulkan_shaders_free_all(void);

#endif
