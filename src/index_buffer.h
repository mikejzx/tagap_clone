#ifndef INDEX_BUFFER_H
#define INDEX_BUFFER_H

#include "types.h"

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
