#ifndef INDEX_BUFFER_H
#define INDEX_BUFFER_H

#include "types.h"

typedef u16 ib_type;
static const VkIndexType IB_VKTYPE = VK_INDEX_TYPE_UINT16;

struct ibuffer
{
    // Vulkan index buffer and memory handles
    VkBuffer vk_buffer;
    VmaAllocation vma_alloc;
    size_t size;
    
    // Indices we want to draw
    u32 index_count;
    u32 first_index;
};

i32 ib_new(struct ibuffer *, const void *, size_t);
void ib_free(struct ibuffer *);

#endif
