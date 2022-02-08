#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#include "types.h"

/*
 * Vertex buffer
 */
struct vbuffer
{
    // Vulkan vertex buffer and memory handles
    VkBuffer vk_buffer;
    VmaAllocation vma_alloc;
    size_t size;
};

/*
 * Vertex attributes
 */
struct vertex
{
    // Vertex positions
    vec2s pos;
};

static const VkVertexInputBindingDescription VERTEX_BINDING_DESC = 
{
    .binding = 0,
    .stride = sizeof(struct vertex),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
};

// Describe all vertex attributes here
static const VkVertexInputAttributeDescription VERTEX_ATTR_DESC[] =
{
    {
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(struct vertex, pos),
    },
};
static const u32 VERTEX_ATTR_COUNT = 
    sizeof(VERTEX_ATTR_DESC) / sizeof(VkVertexInputAttributeDescription);

i32 vb_new(struct vbuffer *, const void *, size_t);
void vb_free(struct vbuffer *);

#endif
