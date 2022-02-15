#ifndef INDEX_BUFFER_H
#define INDEX_BUFFER_H

#include "types.h"

typedef u16 IB_TYPE;
static const VkIndexType IB_VKTYPE = VK_INDEX_TYPE_UINT16;

struct ibuffer
{
    // Vulkan index buffer and memory handles
    VkBuffer vk_buffer;
    VmaAllocation vma_alloc;
    size_t size;
};

i32 ib_new(struct ibuffer *, const void *, size_t);
void ib_free(struct ibuffer *);

#endif
